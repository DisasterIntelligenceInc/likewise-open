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
 *        assoc-marshal.c
 *
 * Abstract:
 *
 *        Association API
 *        Marshalling logic for association-specific datatypes
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */
#include <lwmsg/type.h>
#include <lwmsg/assoc.h>
#include "convert.h"
#include "util-private.h"
#include "assoc-private.h"
#include "data-private.h"

#include <stdlib.h>
#include <string.h>

typedef union
{
    LWMsgHandleID id;
} TransmitData;

typedef struct
{
    LWMsgHandleType type;
    TransmitData data;
} TransmitHandle;

static LWMsgStatus
lwmsg_assoc_marshal_handle(
    LWMsgDataContext* mcontext,
    LWMsgTypeAttrs* attrs,
    void* object,
    void* transmit_object,
    void* data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    void* pointer = NULL;
    TransmitHandle* transmit = transmit_object;
    LWMsgAssoc* assoc = NULL;
    LWMsgSessionManager* manager = NULL;
    LWMsgSession* session = NULL;
    const char* type = NULL;
    const LWMsgContext* context = lwmsg_data_context_get_context(mcontext);

    BAIL_ON_ERROR(status = lwmsg_context_get_data(context, "assoc", (void**) (void*) &assoc));

    BAIL_ON_ERROR(status = lwmsg_assoc_get_session_manager(assoc, &manager));
    BAIL_ON_ERROR(status = assoc->aclass->get_session(assoc, &session));

    pointer = *(void**) object;

    if (pointer != NULL)
    {
        status = lwmsg_session_manager_handle_pointer_to_id(
            manager,
            session,
            pointer,
            &type,
            &transmit->type,
            &transmit->data.id);

        switch (status)
        {
        case LWMSG_STATUS_NOT_FOUND:
            status = LWMSG_STATUS_INVALID_HANDLE;
        default:
            break;
        }

        BAIL_ON_ERROR(status);

        /* Confirm that the handle is of the expected type */
        if (strcmp((const char*) data, type))
        {
            MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                        "Invalid handle 0x%lx(%lu): expected handle of type '%s', "
                        "got '%s'",
                        (unsigned long) pointer,
                        transmit->data.id,
                        (const char*) data,
                        type);
        }

        /* Confirm that handle origin is correct according to type attributes */
        if ((attrs->custom & LWMSG_ASSOC_HANDLE_LOCAL_FOR_RECEIVER && transmit->type != LWMSG_HANDLE_REMOTE) ||
            (attrs->custom & LWMSG_ASSOC_HANDLE_LOCAL_FOR_SENDER && transmit->type != LWMSG_HANDLE_LOCAL))
        {
            if (transmit->type == LWMSG_HANDLE_LOCAL)
            {
                MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                            "Invalid handle 0x%lx(%lu): expected handle which is "
                            "local for the receiver",
                            (unsigned long) pointer,
                            (unsigned long) transmit->data.id);
            }
            else
            {
                MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                            "Invalid handle 0x%lx(%lu): expected handle which is "
                            "local for the sender",
                            (unsigned long) pointer,
                            (unsigned long) transmit->data.id);
            }
        }
    }
    else
    {
        /* If the handle was not supposed to be NULL, raise an error */
        if (attrs->flags & LWMSG_TYPE_FLAG_NOT_NULL)
        {
            MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                        "Invalid handle: expected non-null handle");
        }

        transmit->type = LWMSG_HANDLE_NULL;
    }

cleanup:

    return status;

error:

    goto cleanup;
}

static LWMsgStatus
lwmsg_assoc_unmarshal_handle(
    LWMsgDataContext* mcontext,
    LWMsgTypeAttrs* attrs,
    void* transmit_object,
    void* object,
    void* data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgAssoc* assoc = NULL;
    void* pointer = NULL;
    TransmitHandle* transmit = transmit_object;
    LWMsgSessionManager* manager = NULL;
    LWMsgSession* session = NULL;
    const LWMsgContext* context = lwmsg_data_context_get_context(mcontext);
    LWMsgHandleType location = transmit->type;

    BAIL_ON_ERROR(status = lwmsg_context_get_data(context, "assoc", (void**) (void*) &assoc));

    BAIL_ON_ERROR(status = lwmsg_assoc_get_session_manager(assoc, &manager));
    BAIL_ON_ERROR(status = assoc->aclass->get_session(assoc, &session));

    if (location != LWMSG_HANDLE_NULL)
    {
        /* Invert sense of handle location */
        if (location == LWMSG_HANDLE_LOCAL)
        {
            location = LWMSG_HANDLE_REMOTE;
        }
        else
        {
            location = LWMSG_HANDLE_LOCAL;
        }

        if ((attrs->custom & LWMSG_ASSOC_HANDLE_LOCAL_FOR_RECEIVER && location != LWMSG_HANDLE_LOCAL) ||
            (attrs->custom & LWMSG_ASSOC_HANDLE_LOCAL_FOR_SENDER && location != LWMSG_HANDLE_REMOTE))
        {
            if (location == LWMSG_HANDLE_LOCAL)
            {
                MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                            "Invalid handle (%lu): expected handle which is "
                            "local for the receiver",
                            (unsigned long) pointer,
                            (unsigned long) transmit->data.id);
            }
            else
            {
                MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                            "Invalid handle (%lu): expected handle which is "
                            "local for the sender",
                            (unsigned long) pointer,
                            (unsigned long) transmit->data.id);
            }
        }

        /* Convert handle to pointer */
        status = lwmsg_session_manager_handle_id_to_pointer(
            manager,
            session,
            (const char*) data,
            location,
            transmit->data.id,
            &pointer);

        switch (status)
        {
        case LWMSG_STATUS_NOT_FOUND:
            if (location == LWMSG_HANDLE_REMOTE)
            {
                /* Implicitly register handle seen from the peer for the first time */
                BAIL_ON_ERROR(status = lwmsg_session_manager_register_handle_remote(
                                  manager,
                                  session,
                                  (const char*) data,
                                  transmit->data.id,
                                  NULL,
                                  &pointer));
            }
            else
            {
                MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                                    "Invalid handle (%lu)",
                                    (unsigned long) transmit->data.id);
            }
            break;
        default:
            BAIL_ON_ERROR(status);
        }

        /* Set pointer on unmarshalled object */
        *(void**) object = pointer;
    }
    else
    {
        if (attrs->flags & LWMSG_TYPE_FLAG_NOT_NULL)
        {
            MARSHAL_RAISE_ERROR(mcontext, status = LWMSG_STATUS_INVALID_HANDLE,
                        "Invalid handle: expected non-null handle");
        }
        *(void**) object = NULL;
    }

error:

    return status;
}

static
void
lwmsg_assoc_free_handle(
    LWMsgDataContext* context,
    LWMsgTypeAttrs* attr,
    void* object,
    void* data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgAssoc* assoc = NULL;
    void* pointer = NULL;

    BAIL_ON_ERROR(status = lwmsg_context_get_data(
                      lwmsg_data_context_get_context(context),
                      "assoc",
                      (void**) (void*) &assoc));

    pointer = *(void**) object;

    if (pointer)
    {
        BAIL_ON_ERROR(status = lwmsg_assoc_release_handle(assoc, pointer));
    }

error:

    return;
}

static LWMsgStatus
lwmsg_assoc_print_handle(
    const LWMsgContext* context,
    LWMsgTypeAttrs* attrs,
    void* object,
    LWMsgDataPrintFunction print,
    void* print_data,
    void* data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    void* pointer = NULL;
    LWMsgHandleType location;
    LWMsgHandleID handle;
    LWMsgAssoc* assoc = NULL;
    LWMsgSessionManager* manager = NULL;
    LWMsgSession* session = NULL;
    const char* type;
    char* str = NULL;

    BAIL_ON_ERROR(status = lwmsg_context_get_data(context, "assoc", (void**) (void*) &assoc));

    BAIL_ON_ERROR(status = lwmsg_assoc_get_session_manager(assoc, &manager));
    BAIL_ON_ERROR(status = assoc->aclass->get_session(assoc, &session));

    pointer = *(void**) object;

    if (pointer != NULL)
    {
        status = lwmsg_session_manager_handle_pointer_to_id(
            manager,
            session,
            pointer,
            &type,
            &location,
            &handle);

        switch (status)
        {
        case LWMSG_STATUS_NOT_FOUND:
            status = LWMSG_STATUS_INVALID_HANDLE;
        default:
            break;
        }

        BAIL_ON_ERROR(status);

        /* Confirm that the handle is of the expected type */
        if (strcmp((const char*) data, type))
        {
            BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
        }

        str = lwmsg_format("<%s:%s[%lu]>",
                           type,
                           location == LWMSG_HANDLE_LOCAL ? "local" : "remote",
                           handle);

        BAIL_ON_ERROR(status = print(str, strlen(str), print_data));
    }
    else
    {
        static const char* nullstr = "<null>";
        if (attrs->flags & LWMSG_TYPE_FLAG_NOT_NULL)
        {
            BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
        }

        BAIL_ON_ERROR(status = print(nullstr, strlen(nullstr), print_data));
    }

error:

    if (str)
    {
        free(str);
    }

    return status;
}

static LWMsgTypeSpec handle_id_spec[] =
{
    LWMSG_STRUCT_BEGIN(TransmitHandle),
    LWMSG_MEMBER_UINT8(TransmitHandle, type),
    LWMSG_MEMBER_UNION_BEGIN(TransmitHandle, data),
    LWMSG_MEMBER_UINT32(TransmitData, id),
    LWMSG_ATTR_TAG(LWMSG_HANDLE_LOCAL),
    LWMSG_MEMBER_UINT32(TransmitData, id),
    LWMSG_ATTR_TAG(LWMSG_HANDLE_REMOTE),
    LWMSG_VOID,
    LWMSG_ATTR_TAG(LWMSG_HANDLE_NULL),
    LWMSG_UNION_END,
    LWMSG_ATTR_DISCRIM(TransmitHandle, type),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

LWMsgTypeClass lwmsg_handle_type_class =
{
    .is_pointer = LWMSG_TRUE,
    .transmit_type = handle_id_spec,
    .marshal = lwmsg_assoc_marshal_handle,
    .unmarshal = lwmsg_assoc_unmarshal_handle,
    .destroy_presented = lwmsg_assoc_free_handle,
    .destroy_transmitted = NULL, /* Nothing to free in transmitted form */
    .print = lwmsg_assoc_print_handle
};
