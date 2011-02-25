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
 *        peer-session.c
 *
 * Abstract:
 *
 *        Session management API
 *        Peer session manager implementation
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */
#include <config.h>
#include "session-private.h"
#include "peer-private.h"
#include "util-private.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

typedef struct PeerHandleKey
{
    LWMsgHandleType type;
    LWMsgHandleID id;
} PeerHandleKey;

typedef struct PeerSession
{
    LWMsgSession base;
    /* Session identifier */
    LWMsgSessionID id;
    /* Security token of session creator */
    LWMsgSecurityToken* sec_token;
    /* Reference count */
    size_t volatile refs;
    /* Hash of handles by id */
    LWMsgHashTable handle_by_id;
    /* Lock */
    pthread_mutex_t lock;
    /* Next handle ID */
    unsigned long volatile next_hid;
    /* User data pointer */
    void* data;
    /* Owning peer */
    LWMsgPeer* peer;
    LWMsgSessionDestructFunction destruct;
} PeerSession;

struct LWMsgHandle
{
    /* Key */
    PeerHandleKey key;
    /* Validity bit */
    LWMsgBool volatile valid;
    /* Reference count */
    size_t volatile refs;
    /* Handle type */
    const char* type;
    /* Handle pointer */
    void* pointer;
    /* Handle cleanup function */
    void (*cleanup)(void*);
    /* Link in hash table by id */
    LWMsgRing id_ring;
};

#define SHARED_SESSION(obj) ((PeerSession*) (obj))

static
void*
peer_handle_get_key_id(
    const void* entry
    )
{
    return &((LWMsgHandle*) entry)->key;
}

static
size_t
peer_handle_digest_id(
    const void* key
    )
{
    const PeerHandleKey* hkey = key;

    return hkey->type == LWMSG_HANDLE_LOCAL ? hkey->id : ~hkey->id;
}

static
LWMsgBool
peer_handle_equal_id(
    const void* key1,
    const void* key2
    )
{
    const PeerHandleKey* hkey1 = key1;
    const PeerHandleKey* hkey2 = key2;

    return
        hkey1->type == hkey2->type &&
        hkey1->id == hkey2->id;
}

static inline
void
session_lock(
    PeerSession* session
    )
{
    pthread_mutex_lock(&session->lock);
}

static inline
void
session_unlock(
    PeerSession* session
    )
{
    pthread_mutex_unlock(&session->lock);
}

static void
peer_free_handle(
    PeerSession* session,
    LWMsgHandle* entry
    )
{
    lwmsg_hash_remove_entry(&session->handle_by_id, entry);

    if (entry->cleanup)
    {
        entry->cleanup(entry->pointer);
    }

    free(entry);
}

static
LWMsgStatus
peer_add_handle(
    PeerSession* session,
    const char* type,
    LWMsgHandleType locality,
    void* pointer,
    unsigned long hid,
    void (*cleanup)(void*),
    LWMsgHandle** out_handle
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgHandle* handle = NULL;
    PeerSession* my_session = SHARED_SESSION(session);

    BAIL_ON_ERROR(status = LWMSG_ALLOC(&handle));

    handle->type = type;
    handle->valid = LWMSG_TRUE;
    handle->refs = 1;
    handle->pointer = pointer;
    handle->cleanup = cleanup;
    handle->key.id = hid;
    handle->key.type = locality;

    lwmsg_ring_init(&handle->id_ring);

    lwmsg_hash_insert_entry(&my_session->handle_by_id, handle);

    *out_handle = handle;

error:

    return status;
}

static
void
peer_free_session(
    PeerSession* session
    )
{
    LWMsgHandle* handle = NULL;
    LWMsgHashIter iter = {0};

    lwmsg_hash_iter_begin(&session->handle_by_id, &iter);
    while ((handle = lwmsg_hash_iter_next(&session->handle_by_id, &iter)))
    {
        peer_free_handle(session, handle);
    }
    lwmsg_hash_iter_end(&session->handle_by_id, &iter);

    lwmsg_hash_destroy(&session->handle_by_id);

    if (session->destruct && session->data)
    {
        session->destruct(session->sec_token, session->data);
    }

    if (session->sec_token)
    {
        lwmsg_security_token_delete(session->sec_token);
    }

    pthread_mutex_destroy(&session->lock);
    free(session);
}

static
void
peer_session_reset_in_lock(
    PeerSession* my_session
    )
{
    LWMsgHandle* handle = NULL;
    LWMsgHashIter iter = {0};

    lwmsg_hash_iter_begin(&my_session->handle_by_id, &iter);
    while ((handle = lwmsg_hash_iter_next(&my_session->handle_by_id, &iter)))
    {
        handle->valid = LWMSG_FALSE;
        if (handle->cleanup && handle->pointer)
        {
            handle->cleanup(handle->pointer);
            handle->cleanup = NULL;
            handle->pointer = NULL;
        }
        lwmsg_hash_remove_entry(&my_session->handle_by_id, handle);
    }
    lwmsg_hash_iter_end(&my_session->handle_by_id, &iter);

    if (my_session->destruct && my_session->data)
    {
        my_session->destruct(my_session->sec_token, my_session->data);
        my_session->data = NULL;
        my_session->destruct = NULL;
    }
}

void
lwmsg_peer_session_reset(
    LWMsgSession* session
    )
{
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);
    peer_session_reset_in_lock(my_session);
    session_unlock(my_session);
}

static
LWMsgStatus
peer_accept(
    LWMsgSession* session,
    const LWMsgSessionCookie* connect,
    LWMsgSecurityToken* token
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    PeerSession* my_session = SHARED_SESSION(session);
    LWMsgPeer* peer = NULL;

    /* FIXME: somewhat hacky */
    if (my_session->peer->connect_session == session &&
        my_session->peer->direct)
    {
        peer = my_session->peer->direct->endpoint->server;
    }
    else
    {
        peer = my_session->peer;
    }

    if (peer->session_construct)
    {
        BAIL_ON_ERROR(status = peer->session_construct(
            token,
            my_session->peer->session_construct_data,
            &my_session->data));
        my_session->destruct = peer->session_destruct;
    }

    my_session->sec_token = token;
    my_session->refs++;
    memcpy(my_session->id.connect.bytes, connect->bytes, sizeof(connect->bytes));
    lwmsg_session_generate_cookie(&my_session->id.accept);

done:

    return status;

error:

    if (my_session)
    {
        peer_free_session(my_session);
    }

    goto done;
}

static
LWMsgStatus
peer_connect(
    LWMsgSession* session,
    const LWMsgSessionCookie* accept,
    LWMsgSecurityToken* token
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    PeerSession* ssession = SHARED_SESSION(session);

    session_lock(ssession);

    if (ssession->sec_token)
    {
        /* Session has been connected before */
        if (!lwmsg_security_token_can_access(ssession->sec_token, token))
        {
            BAIL_ON_ERROR(status = LWMSG_STATUS_SECURITY);
        }

        /* Has session state been lost? */
        if (memcmp(&ssession->id.accept.bytes, accept->bytes, sizeof(accept->bytes)) &&
            lwmsg_hash_get_count(&ssession->handle_by_id) > 0)
        {
            /* Reset the session */
            peer_session_reset_in_lock(ssession);
        }

        lwmsg_security_token_delete(token);
        /* Update accept cookie if it changed */
        memcpy(ssession->id.accept.bytes, accept->bytes, sizeof(accept->bytes));
    }
    else
    {
        /* Session has not been connected bfore */
        ssession->sec_token = token;
        memcpy(ssession->id.accept.bytes, accept->bytes, sizeof(accept->bytes));
    }

    ssession->refs++;

done:

    session_unlock(ssession);

    return status;

error:

    goto done;
}

static
void
peer_release(
    LWMsgSession* session
    )
{
    PeerSession* my_session = SHARED_SESSION(session);

    my_session->refs--;

    if (my_session->refs == 0)
    {
        peer_free_session(my_session);
    }
}

static
LWMsgSecurityToken*
peer_get_peer_security_token (
    LWMsgSession* session
    )
{
    PeerSession* my_session = SHARED_SESSION(session);

    return my_session->sec_token;
}

static
LWMsgHandle*
peer_find_handle_by_id(
    PeerSession* session,
    LWMsgHandleType type,
    LWMsgHandleID id
    )
{
    PeerHandleKey key = {0};

    key.type = type;
    key.id = id;

    return lwmsg_hash_find_key(&session->handle_by_id, &key);
}

static
LWMsgStatus
peer_register_handle_remote(
    LWMsgSession* session,
    const char* type,
    LWMsgHandleID hid,
    void (*cleanup)(void* ptr),
    LWMsgHandle** handle)
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgHandle* my_handle = NULL;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    BAIL_ON_ERROR(status = peer_add_handle(
                      my_session,
                      type,
                      LWMSG_HANDLE_REMOTE,
                      NULL,
                      hid,
                      cleanup,
                      &my_handle));

    if (handle)
    {
        *handle = my_handle;
    }

error:

    session_unlock(my_session);

    return status;
}

static
LWMsgStatus
peer_register_handle_local(
    LWMsgSession* session,
    const char* type,
    void* data,
    void (*cleanup)(void* ptr),
    LWMsgHandle** handle
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgHandle* my_handle = NULL;
    PeerSession* my_session = SHARED_SESSION(session);
    PeerHandleKey key = {0};

    if (!data)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_PARAMETER);
    }

    session_lock(my_session);

    if (lwmsg_hash_get_count(&my_session->handle_by_id) == UINT32_MAX)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_RESOURCE_LIMIT);
    }

    do
    {
        key.type = LWMSG_HANDLE_LOCAL;
        key.id = my_session->next_hid++;
    } while (lwmsg_hash_find_key(&my_session->handle_by_id, &key));

    BAIL_ON_ERROR(status = peer_add_handle(
                      my_session,
                      type,
                      LWMSG_HANDLE_LOCAL,
                      data,
                      key.id,
                      cleanup,
                      &my_handle));

    if (handle)
    {
        *handle = my_handle;
    }

error:

    session_unlock(my_session);

    return status;
}

static
void
peer_retain_handle(
    LWMsgSession* session,
    LWMsgHandle* handle
    )
{
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    handle->refs++;

    session_unlock(my_session);
}

static
void
peer_release_handle(
    LWMsgSession* session,
    LWMsgHandle* handle
    )
{
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    if (--handle->refs == 0)
    {
        peer_free_handle(my_session, handle);
    }

    session_unlock(my_session);
}

static
LWMsgStatus
peer_unregister_handle(
    LWMsgSession* session,
    LWMsgHandle* handle
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    if (!handle->valid)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
    }

    handle->valid = LWMSG_FALSE;

    if (--handle->refs == 0)
    {
        peer_free_handle(my_session, handle);
    }

done:

    session_unlock(my_session);

    return status;

error:

    goto done;
}

static
LWMsgStatus
peer_resolve_handle_to_id(
    LWMsgSession* session,
    LWMsgHandle* handle,
    const char** type,
    LWMsgHandleType* htype,
    LWMsgHandleID* hid
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    PeerSession* my_session = SHARED_SESSION(session);

    if (!handle)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
    }

    session_lock(my_session);

    if (!handle->valid)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
    }

    if (type)
    {
        *type = handle->type;
    }

    if (htype)
    {
        *htype = handle->key.type;
    }

    if (hid)
    {
        *hid = handle->key.id;
    }

done:

    session_unlock(my_session);

    return status;

error:

    goto done;
}

static
LWMsgStatus
peer_resolve_id_to_handle(
    LWMsgSession* session,
    const char* type,
    LWMsgHandleType htype,
    LWMsgHandleID hid,
    LWMsgHandle** handle
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    LWMsgHandle* my_handle = NULL;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    my_handle = peer_find_handle_by_id(my_session, htype, hid);

    if (!my_handle)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_NOT_FOUND);
    }

    if (!my_handle->valid || (type && strcmp(type, my_handle->type)))
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
    }

    *handle = my_handle;
    my_handle->refs++;

done:

    session_unlock(my_session);

    return status;

error:

    goto done;
}

static
LWMsgStatus
peer_get_handle_data(
    LWMsgSession* session,
    LWMsgHandle* handle,
    void** data
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    if (!handle->valid)
    {
        BAIL_ON_ERROR(status = LWMSG_STATUS_INVALID_HANDLE);
    }

    if (data)
    {
        *data = handle->pointer;
    }

done:

    session_unlock(my_session);

    return status;

error:

    goto done;
}

static
void*
peer_get_data (
    LWMsgSession* session
    )
{
    void* data = NULL;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);

    data = my_session->data;

    session_unlock(my_session);

    return data;
}

static
const LWMsgSessionID*
peer_get_id(
    LWMsgSession* session
    )
{
    return &SHARED_SESSION(session)->id;
}

static
size_t
peer_get_assoc_count(
    LWMsgSession* session
    )
{
    size_t refs;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);
    refs = my_session->refs;
    session_unlock(my_session);
    return refs;
}

static
size_t
peer_get_handle_count(
    LWMsgSession* session
    )
{
    size_t handles;
    PeerSession* my_session = SHARED_SESSION(session);

    session_lock(my_session);
    handles = lwmsg_hash_get_count(&my_session->handle_by_id);
    session_unlock(my_session);
    return handles;
}

static LWMsgSessionClass peer_class =
{
    .accept = peer_accept,
    .connect = peer_connect,
    .release = peer_release,
    .register_handle_local = peer_register_handle_local,
    .register_handle_remote = peer_register_handle_remote,
    .retain_handle = peer_retain_handle,
    .release_handle = peer_release_handle,
    .unregister_handle = peer_unregister_handle,
    .resolve_handle_to_id = peer_resolve_handle_to_id,
    .resolve_id_to_handle = peer_resolve_id_to_handle,
    .get_handle_data = peer_get_handle_data,
    .get_data = peer_get_data,
    .get_id = peer_get_id,
    .get_assoc_count = peer_get_assoc_count,
    .get_handle_count = peer_get_handle_count,
    .get_peer_security_token = peer_get_peer_security_token
};

LWMsgStatus
lwmsg_peer_session_new(
    LWMsgPeer* peer,
    LWMsgSession** out_session
    )
{
    LWMsgStatus status = LWMSG_STATUS_SUCCESS;
    PeerSession* session = NULL;

    BAIL_ON_ERROR(status = LWMSG_ALLOC(&session));

    session->base.sclass = &peer_class;
    session->peer = peer;

    BAIL_ON_ERROR(status = lwmsg_hash_init(
                      &session->handle_by_id,
                      31,
                      peer_handle_get_key_id,
                      peer_handle_digest_id,
                      peer_handle_equal_id,
                      offsetof(LWMsgHandle, id_ring)));

    lwmsg_session_generate_cookie(&session->id.connect);

    session->refs = 1;

    *out_session = LWMSG_SESSION(session);

done:

    return status;

error:

    if (session)
    {
        peer_free_session(session);
    }

    goto done;
}
