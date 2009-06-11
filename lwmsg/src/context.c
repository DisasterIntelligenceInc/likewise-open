/*
 * Copyright (c) Likewise Software.  All rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Module Name:
 *
 *        context.c
 *
 * Abstract:
 *
 *        Marshalling context API
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */
#include "context-private.h"
#include "status-private.h"
#include "util-private.h"
#include "type-private.h"
#include <lwmsg/type.h>

#include <stdlib.h>

static LWMsgStatus
lwmsg_context_default_alloc (
    size_t size,
    void** out,
    void* data)
{
    void* object = malloc(size == 0 ? 1 : size);

    if (!object)
    {
        return LWMSG_STATUS_MEMORY;
    }
    else
    {
        memset(object, 0, size);
        *out = object;
        return LWMSG_STATUS_SUCCESS;
    }
}

static
void
lwmsg_context_default_free (
    void* object,
    void* data
    )
{
    free(object);
}

static LWMsgStatus
lwmsg_context_default_realloc (
    void* object,
    size_t old_size,
    size_t new_size,
    void** new_object,
    void* data)
{
    void* nobj = realloc(object, new_size);

    if (!nobj)
    {
        return LWMSG_STATUS_MEMORY;
    }
    else
    {
        if (new_size > old_size)
        {
            memset(nobj + old_size, 0, new_size - old_size);
        }
        *new_object = nobj;
        return LWMSG_STATUS_SUCCESS;
    }
}

void
lwmsg_context_setup(
    LWMsgContext* context,
    const LWMsgContext* parent
    )
{
    context->parent = parent;
}

LWMsgStatus
lwmsg_context_new(
    const LWMsgContext* parent,
    LWMsgContext** context
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgContext* my_context = NULL;

    my_context = calloc(1, sizeof(*my_context));

    if (my_context == NULL)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_MEMORY);
    }

    lwmsg_context_setup(my_context, parent);

    *context = my_context;

error:

    return status;
}

void
lwmsg_context_cleanup(LWMsgContext* context)
{
    lwmsg_error_clear(&context->error);
}

void
lwmsg_context_delete(LWMsgContext* context)
{
    lwmsg_context_cleanup(context);
    free(context);
}

void
lwmsg_context_set_memory_functions(
    LWMsgContext* context,
    LWMsgAllocFunction alloc,
    LWMsgFreeFunction free,
    LWMsgReallocFunction realloc,
    void* data
    )
{
    context->alloc = alloc;
    context->free = free;
    context->realloc = realloc;
    context->memdata = data;
}

const char*
lwmsg_context_get_error_message(
    LWMsgContext* context,
    LWMsgStatus status
    )
{
    return lwmsg_error_message(status, &context->error);
}

void
lwmsg_context_get_memory_functions(
    const LWMsgContext* context,
    LWMsgAllocFunction* alloc,
    LWMsgFreeFunction* free,
    LWMsgReallocFunction* realloc,
    void** data
    )
{
    if (!context)
    {
        if (alloc)
        {
            *alloc = lwmsg_context_default_alloc;
        }
        if (free)
        {
            *free = lwmsg_context_default_free;
        }
        if (realloc)
        {
            *realloc = lwmsg_context_default_realloc;
        }
        if (data)
        {
            *data = NULL;
        }
    }
    else if (context->alloc)
    {
        if (alloc)
        {
            *alloc = context->alloc;
        }
        if (free)
        {
            *free = context->free;
        }
        if (realloc)
        {
            *realloc = context->realloc;
        }
        if (data)
        {
            *data = context->memdata;
        }
    }
    else
    {
        lwmsg_context_get_memory_functions(context->parent, alloc, free, realloc, data);
    }
}

void
lwmsg_context_set_data_function(
    LWMsgContext* context,
    LWMsgContextDataFunction fn,
    void* data
    )
{
    context->datafn = fn;
    context->datafndata = data;
}

void
lwmsg_context_set_log_function(
    LWMsgContext* context,
    LWMsgLogFunction logfn,
    void* data
    )
{
    context->logfn = logfn;
    context->logfndata = data;
}

static
void
lwmsg_context_get_log_function(
    const LWMsgContext* context,
    LWMsgLogFunction* logfn,
    void** logfndata
    )
{
    if (!context)
    {
        *logfn = NULL;
        *logfndata = NULL;
    }
    else if (context->logfn)
    {
        *logfn = context->logfn;
        *logfndata = context->logfndata;
    }
    else
    {
        lwmsg_context_get_log_function(context->parent, logfn, logfndata);
    }
}


typedef struct freeinfo
{
    const LWMsgContext* context;
    LWMsgFreeFunction free;
    void* data;
} freeinfo;

static
LWMsgStatus
lwmsg_context_free_graph_visit(
    LWMsgTypeIter* iter,
    unsigned char* object,
    void* data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    freeinfo* info = (freeinfo*) data;

    switch(iter->kind)
    {
    case LWMSG_KIND_CUSTOM:
        if (iter->info.kind_custom.typeclass->free)
        {
            iter->info.kind_custom.typeclass->free(
                info->context,
                iter->size,
                &iter->attrs,
                object,
                iter->info.kind_custom.typedata);
        }
        break;
    case LWMSG_KIND_POINTER:
        BAIL_ON_ERROR(status = lwmsg_type_visit_graph_children(
                          iter,
                          object,
                          lwmsg_context_free_graph_visit,
                          data));
        info->free(*(void **) object, info->data);
        break;
    default:
        BAIL_ON_ERROR(status = lwmsg_type_visit_graph_children(
                              iter,
                              object,
                              lwmsg_context_free_graph_visit,
                              data));
        break;
    }

error:

    return status;
}

LWMsgStatus
lwmsg_context_free_graph_internal(
    const LWMsgContext* context,
    LWMsgTypeIter* iter,
    unsigned char* object)
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    freeinfo info;

    lwmsg_context_get_memory_functions(context, NULL, &info.free, NULL, &info.data);
    info.context = context;

    BAIL_ON_ERROR(status = lwmsg_type_visit_graph(
                      iter,
                      object,
                      lwmsg_context_free_graph_visit,
                      &info));

error:

    return status;
}

LWMsgStatus
lwmsg_context_free_graph(
    const LWMsgContext* context,
    LWMsgTypeSpec* type,
    void* root)
{
    LWMsgTypeIter iter;

    lwmsg_type_iterate_promoted(type, &iter);

    return lwmsg_context_free_graph_internal(context, &iter, (unsigned char*) &root);
}

LWMsgStatus
lwmsg_context_get_data(
    const LWMsgContext* context,
    const char* key,
    void** out_data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;

    for (; context; context = context->parent)
    {
        if (context->datafn)
        {
            status = context->datafn(key, out_data, context->datafndata);
            if (status == LWMSG_STATUS_NOT_FOUND)
            {
                status = LWMSG_STATUS_SUCCESS;
                continue;
            }
            BAIL_ON_ERROR(status);
            break;
        }
    }

error:

    return status;
}

void
lwmsg_context_log(
    const LWMsgContext* context,
    LWMsgLogLevel level,
    const char* message,
    const char* filename,
    unsigned int line
    )
{
    LWMsgLogFunction logfn = NULL;
    void* logfndata = NULL;

    lwmsg_context_get_log_function(context, &logfn, &logfndata);

    if (logfn)
    {
        logfn(level, message, filename, line, logfndata);
    }
}

void
lwmsg_context_log_printf(
    const LWMsgContext* context,
    LWMsgLogLevel level,
    const char* filename,
    unsigned int line,
    const char* format,
    ...
    )
{
    LWMsgLogFunction logfn = NULL;
    void* logfndata = NULL;
    char* message = NULL;
    va_list ap;

    lwmsg_context_get_log_function(context, &logfn, &logfndata);

    if (logfn)
    {
        va_start(ap, format);
        message = lwmsg_formatv(format, ap);
        va_end(ap);

        if (message)
        {
            logfn(level, message, filename, line, logfndata);
            free(message);
        }
    }
}
