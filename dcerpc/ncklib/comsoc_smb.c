#include <commonp.h>
#include <com.h>
#include <comprot.h>
#include <comnaf.h>
#include <comp.h>
#include <comsoc_smb.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cnp.h>
#include <npnaf.h>
#include <stddef.h>

#include <lwio/lwio.h>
#include <lw/base.h>

#define SMB_SOCKET_LOCK(sock) (rpc__smb_socket_lock(sock))
#define SMB_SOCKET_UNLOCK(sock) (rpc__smb_socket_unlock(sock))

typedef enum rpc_smb_state_e
{
    SMB_STATE_SEND,
    SMB_STATE_RECV,
    SMB_STATE_LISTEN,
    SMB_STATE_ERROR
} rpc_smb_state_t;

typedef struct rpc_smb_buffer_s
{
    size_t capacity;
    unsigned char* base;
    unsigned char* start_cursor;
    unsigned char* end_cursor;
} rpc_smb_buffer_t, *rpc_smb_buffer_p_t;

typedef struct rpc_smb_socket_s
{
    rpc_smb_state_t volatile state;
    rpc_np_addr_t peeraddr;
    rpc_np_addr_t localaddr;
    PIO_CONTEXT context;
    IO_FILE_HANDLE np;
    rpc_smb_buffer_t sendbuffer;
    rpc_smb_buffer_t recvbuffer;
    struct
    {
        IO_FILE_HANDLE* queue;
        size_t capacity;
        size_t length;
        int selectfd[2];
    } accept_backlog;
    dcethread* listen_thread;
    dcethread_mutex lock;
    dcethread_cond event;
} rpc_smb_socket_t, *rpc_smb_socket_p_t;

INTERNAL
inline
size_t
rpc__smb_buffer_pending(
    rpc_smb_buffer_p_t buffer
    )
{
    return buffer->end_cursor - buffer->start_cursor;
}

INTERNAL
inline
size_t
rpc__smb_buffer_available(
    rpc_smb_buffer_p_t buffer
    )
{
    return (buffer->base + buffer->capacity) - buffer->end_cursor;
}

INTERNAL
inline
rpc_socket_error_t
rpc__smb_buffer_ensure_available(
    rpc_smb_buffer_p_t buffer,
    size_t space
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    unsigned char* new_base = NULL;

    if (!buffer->base)
    {
        buffer->capacity = 2048;
        buffer->base = malloc(buffer->capacity);

        if (!buffer->base)
        {
            serr = ENOMEM;
            goto error;
        }

        buffer->end_cursor = buffer->start_cursor = buffer->base;
    }

    if (space > rpc__smb_buffer_available(buffer))
    {
        while (space > rpc__smb_buffer_available(buffer))
        {
            buffer->capacity *= 2;
        }

        new_base = realloc(buffer->base, buffer->capacity);

        if (!new_base)
        {
            serr = ENOMEM;
            goto error;
        }

        buffer->start_cursor = new_base + (buffer->start_cursor - buffer->base);
        buffer->end_cursor = new_base + (buffer->end_cursor - buffer->base);

        buffer->base = new_base;
    }

error:

    return serr;
}

INTERNAL
inline
size_t
rpc__smb_buffer_packet_size(
    rpc_smb_buffer_p_t buffer
    )
{
    rpc_cn_common_hdr_p_t packet = (rpc_cn_common_hdr_p_t) buffer->start_cursor;
    uint16_t result;

    if (rpc__smb_buffer_pending(buffer) < sizeof(*packet))
    {
        return sizeof(*packet);
    }
    else
    {
        int packet_order = ((packet->drep[0] >> 4) & 1);
        int native_order = (NDR_LOCAL_INT_REP == ndr_c_int_big_endian) ? 0 : 1;

        if (packet_order != native_order)
        {
            result = SWAB_16(packet->frag_len);
        }
        else
        {
            result = packet->frag_len;
        }

        return (size_t) result;
    }
}

INTERNAL
inline
boolean
rpc__smb_buffer_packet_is_last(
    rpc_smb_buffer_p_t buffer
    )
{
    rpc_cn_common_hdr_p_t packet = (rpc_cn_common_hdr_p_t) buffer->start_cursor;

    return (packet->flags & RPC_C_CN_FLAGS_LAST_FRAG) == RPC_C_CN_FLAGS_LAST_FRAG;
}

INTERNAL
inline
rpc_socket_error_t
rpc__smb_buffer_append(
    rpc_smb_buffer_p_t buffer,
    void* data,
    size_t data_size
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    serr = rpc__smb_buffer_ensure_available(buffer, data_size);
    if (serr)
    {
        goto error;
    }

    memcpy(buffer->end_cursor, data, data_size);

    buffer->end_cursor += data_size;

error:

    return serr;
}

INTERNAL
inline
void
rpc__smb_buffer_settle(
    rpc_smb_buffer_p_t buffer
    )
{
    size_t filled = buffer->end_cursor - buffer->start_cursor;
    memmove(buffer->base, buffer->start_cursor, filled);
    buffer->start_cursor = buffer->base;
    buffer->end_cursor = buffer->base + filled;
}

/* Advance buffer start_cursor to the end of the last packet
   or the last packet that is the final fragment in a series,
   whichever comes first.  If the final fragment is found,
   return true, otherwise false.
*/
INTERNAL
inline
boolean
rpc__smb_buffer_advance_cursor(rpc_smb_buffer_p_t buffer, size_t* amount)
{
    boolean last;
    size_t packet_size;

    while (rpc__smb_buffer_packet_size(buffer) <= rpc__smb_buffer_pending(buffer))
    {
        last = rpc__smb_buffer_packet_is_last(buffer);
        packet_size = rpc__smb_buffer_packet_size(buffer);

        buffer->start_cursor += packet_size;

        if (last)
        {
            *amount = buffer->start_cursor - buffer->base;

            return true;
        }
    }

    return false;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_create(
    rpc_smb_socket_p_t* out
    )
{
    rpc_smb_socket_p_t sock = NULL;
    int err = 0;

    sock = calloc(1, sizeof(*sock));

    if (!sock)
    {
        err = ENOMEM;
        goto done;
    }


    sock->accept_backlog.selectfd[0] = -1;
    sock->accept_backlog.selectfd[1] = -1;

    /* Set up reasonable default local endpoint */
    sock->localaddr.rpc_protseq_id = RPC_C_PROTSEQ_ID_NCACN_NP;
    sock->localaddr.len = offsetof(rpc_np_addr_t, remote_host) + sizeof(sock->localaddr.remote_host);
    sock->localaddr.sa.sun_family = AF_UNIX;
    sock->localaddr.sa.sun_path[0] = '\0';
    sock->localaddr.remote_host[0] = '\0';

    dcethread_mutex_init_throw(&sock->lock, NULL);
    dcethread_cond_init_throw(&sock->event, NULL);

    err = LwNtStatusToUnixErrno(LwIoOpenContextShared(&sock->context));
    if (err)
    {
        goto error;
    }

    *out = sock;

done:

    return err;

error:

    if (sock)
    {
        if (sock->context)
        {
            LwIoCloseContext(sock->context);
        }

        dcethread_mutex_destroy_throw(&sock->lock);
        dcethread_cond_destroy_throw(&sock->event);
    }

    goto done;
}

INTERNAL
void
rpc__smb_socket_destroy(
    rpc_smb_socket_p_t sock
    )
{
    size_t i;

    if (sock)
    {
        if (sock->accept_backlog.queue)
        {
            for (i = 0; i < sock->accept_backlog.capacity; i++)
            {
                if (sock->accept_backlog.queue[i])
                {
                    NtCtxCloseFile(sock->context, sock->accept_backlog.queue[i]);
                }
            }

            close(sock->accept_backlog.selectfd[0]);
            close(sock->accept_backlog.selectfd[1]);

            free(sock->accept_backlog.queue);
        }

        if (sock->np && sock->context)
        {
            NtCtxCloseFile(sock->context, sock->np);
        }

        if (sock->context)
        {
            LwIoCloseContext(sock->context);
        }

        if (sock->sendbuffer.base)
        {
            free(sock->sendbuffer.base);
        }

        if (sock->recvbuffer.base)
        {
            free(sock->recvbuffer.base);
        }

        dcethread_mutex_destroy_throw(&sock->lock);
        dcethread_cond_destroy_throw(&sock->event);

        free(sock);
    }

    return;
}

INTERNAL
inline
void
rpc__smb_socket_lock(
    rpc_smb_socket_p_t sock
    )
{
    dcethread_mutex_lock_throw(&sock->lock);
}

INTERNAL
inline
void
rpc__smb_socket_unlock(
    rpc_smb_socket_p_t sock
    )
{
    dcethread_mutex_unlock_throw(&sock->lock);
}

INTERNAL
inline
void
rpc__smb_socket_change_state(
    rpc_smb_socket_p_t sock,
    rpc_smb_state_t state
    )
{
    sock->state = state;
    dcethread_cond_broadcast_throw(&sock->event);
}

INTERNAL
inline
void
rpc__smb_socket_wait(
    rpc_smb_socket_p_t sock
    )
{
    DCETHREAD_TRY
    {
        dcethread_cond_wait_throw(&sock->event, &sock->lock);
    }
    DCETHREAD_CATCH_ALL(e)
    {
        dcethread_mutex_unlock(&sock->lock);
        DCETHREAD_RAISE(*e);
    }
    DCETHREAD_ENDTRY;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_construct(
    rpc_socket_t sock,
    rpc_protseq_id_t pseq_id ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb_sock = NULL;

    serr = rpc__smb_socket_create(&smb_sock);

    if (serr)
    {
        goto error;
    }

    sock->data.pointer = (void*) smb_sock;

done:

    return serr;

error:

    if (smb_sock)
    {
        rpc__smb_socket_destroy(smb_sock);
    }

    goto done;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_destruct(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    rpc__smb_socket_destroy((rpc_smb_socket_p_t) sock->data.pointer);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_bind(
    rpc_socket_t sock,
    rpc_addr_p_t addr
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_np_addr_p_t npaddr = (rpc_np_addr_p_t) addr;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    smb->localaddr = *npaddr;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_session_key(
    rpc_cn_assoc_t *assoc,
    size_t len,
    unsigned char* key
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_np_auth_info_t *np_auth = NULL;

    RPC_MEM_ALLOC(np_auth, rpc_np_auth_info_t*, sizeof(rpc_np_auth_info_t), RPC_C_MEM_NP_SEC_CONTEXT, 0);

    if (!np_auth)
    {
        serr = ENOMEM;
        goto error;
    }

    np_auth->refcount = 0;
    np_auth->princ_name = NULL;
    np_auth->workstation = NULL;
    np_auth->session_key_len = len;

    RPC_MEM_ALLOC(np_auth->session_key, unsigned char*,
                  (sizeof(unsigned char)*(len+1)),
                  RPC_C_MEM_NP_SEC_CONTEXT, 0);

    if (!np_auth->session_key)
    {
        serr = ENOMEM;
        goto error;
    }

    memcpy(np_auth->session_key, key, len);

    assoc->security.assoc_named_pipe_info = np_auth;

    np_auth->refcount++;

done:

    return serr;

error:

    if (np_auth)
    {
        if (np_auth->session_key)
        {
            RPC_MEM_FREE(np_auth->session_key, RPC_C_MEM_NP_SEC_CONTEXT);
        }

        RPC_MEM_FREE(np_auth, RPC_C_MEM_NP_SEC_CONTEXT);
    }

    goto done;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_connect(
    rpc_socket_t sock,
    rpc_addr_p_t addr,
    rpc_cn_assoc_t *assoc ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    char *netaddr, *endpoint, *pipename;
    unsigned32 dbg_status = 0;
    PSTR smbpath = NULL;
    PIO_ACCESS_TOKEN acctoken = NULL;
    PBYTE sesskey = NULL;
    USHORT sesskeylen = 0;
    IO_FILE_NAME filename;
    IO_STATUS_BLOCK io_status;

    SMB_SOCKET_LOCK(smb);

    /* Break address into host and endpoint */
    rpc__naf_addr_inq_netaddr (addr,
                               (unsigned_char_t**) &netaddr,
                               &dbg_status);
    rpc__naf_addr_inq_endpoint (addr,
                                (unsigned_char_t**) &endpoint,
                                &dbg_status);

    if (!strncmp(endpoint, "\\pipe\\", sizeof("\\pipe\\") - 1) ||
        !strncmp(endpoint, "\\PIPE\\", sizeof("\\PIPE\\") - 1))
    {
        pipename = endpoint + sizeof("\\pipe\\") - 1;
    }
    else
    {
        serr = EINVAL;
        goto error;
    }

    serr = NtStatusToUnixErrno(
        LwRtlCStringAllocatePrintf(
            &smbpath,
            "\\rdr\\%s\\IPC$\\%s",
            (char*) netaddr,
            (char*) pipename));
    if (serr)
    {
        goto error;
    }

    filename.RootFileHandle = NULL;
    filename.IoNameOptions = 0;

    serr = NtStatusToUnixErrno(
        LwRtlWC16StringAllocateFromCString(
            &filename.FileName,
            smbpath));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToUnixErrno(
        LwIoGetThreadAccessToken(&acctoken));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToUnixErrno(
        NtCtxCreateFile(
            smb->context,                    /* IO context */
            acctoken,                        /* Security token */
            &smb->np,                        /* Created handle */
            NULL,                            /* Async control block */
            &io_status,                      /* Status block ??? */
            &filename,                       /* Filename */
            NULL,                            /* Security descriptor */
            NULL,                            /* Security QOS */
            GENERIC_READ | GENERIC_WRITE,    /* Access mode */
            0,                               /* Allocation size */
            0,                               /* File attributes */
            SHARE_WRITE | SHARE_READ,        /* Sharing mode */
            OPEN_EXISTING,                   /* Create disposition */
            0,                               /* Create options */
            NULL,                            /* EA buffer */
            0,                               /* EA buffer length */
            NULL                             /* ECP List */
            ));
    if (serr)
    {
        goto error;
    }

    serr = NtStatusToUnixErrno(
        LwIoCtxGetSessionKey(
            smb->context,
            smb->np,
            &sesskeylen,
            &sesskey));
    if (serr)
    {
        goto error;
    }

    serr = rpc__smb_socket_set_session_key(
        assoc,
        (size_t) sesskeylen,
        (unsigned char*) sesskey);
    if (serr)
    {
        goto error;
    }

    /* Save address for future inquiries on this socket */
    memcpy(&smb->peeraddr, addr, sizeof(smb->peeraddr));

    /* Since we did a connect, we will be sending first */
    smb->state = SMB_STATE_SEND;

done:

    if (acctoken != NULL)
    {
        LwIoDeleteAccessToken(acctoken);
    }

    if (sesskey)
    {
        RtlMemoryFree(sesskey);
    }

    if (filename.FileName)
    {
        RtlMemoryFree(filename.FileName);
    }

    if (smbpath)
    {
        RtlMemoryFree(smbpath);
    }

    SMB_SOCKET_UNLOCK(smb);

    // rpc_string_free handles when *ptr is NULL
    rpc_string_free((unsigned_char_t**) &netaddr, &dbg_status);
    rpc_string_free((unsigned_char_t**) &endpoint, &dbg_status);

    return serr;

error:

    goto done;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_accept(
    rpc_socket_t sock,
    rpc_addr_p_t addr ATTRIBUTE_UNUSED,
    rpc_socket_t *newsock
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    rpc_socket_t npsock = NULL;
    rpc_smb_socket_p_t npsmb = NULL;
    IO_FILE_HANDLE np = NULL;
    size_t i;
    char c = 0;

    SMB_SOCKET_LOCK(smb);

    while (smb->accept_backlog.length == 0)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            serr = -1;
            goto error;
        }

        rpc__smb_socket_wait(smb);
    }

    for (i = 0; i < smb->accept_backlog.capacity; i++)
    {
        if (smb->accept_backlog.queue[i] != NULL)
        {
            np = smb->accept_backlog.queue[i];
            smb->accept_backlog.queue[i] = NULL;
            smb->accept_backlog.length--;
            if (read(smb->accept_backlog.selectfd[0], &c, sizeof(c)) != c)
            {
                serr = errno;
                goto error;
            }
            dcethread_cond_broadcast_throw(&smb->event);
        }
    }

    serr = rpc__socket_open(sock->pseq_id, &npsock);
    if (serr)
    {
        goto error;
    }

    npsmb = (rpc_smb_socket_p_t) npsock->data.pointer;

    npsmb->np = np;
    np = NULL;

    npsmb->state = SMB_STATE_RECV;

    memcpy(&npsmb->localaddr, &smb->localaddr, sizeof(npsmb->localaddr));

    /* FIXME: query for and use real peer address */
    memcpy(&npsmb->peeraddr, &smb->localaddr, sizeof(npsmb->peeraddr));
    memcpy(addr, &npsmb->peeraddr, sizeof(npsmb->peeraddr));

    /* FIXME: set up session key */

    *newsock = npsock;

error:

    if (np)
    {
        NtCtxCloseFile(smb->context, np);
    }

    SMB_SOCKET_UNLOCK(smb);

    return serr;
}

INTERNAL
void*
rpc__smb_socket_listen_thread(void* data)
{
    int serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) data;
    IO_STATUS_BLOCK status_block;
    char *endpoint, *pipename;
    unsigned32 dbg_status = 0;
    PSTR smbpath = NULL;
    IO_FILE_NAME filename;
    size_t i;
    char c = 0;
    LONG64 default_timeout = 0;

    SMB_SOCKET_LOCK(smb);

    while (smb->state != SMB_STATE_LISTEN)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            goto error;
        }

        rpc__smb_socket_wait(smb);
    }

    /* Extract endpoint */
    rpc__naf_addr_inq_endpoint ((rpc_addr_p_t) &smb->localaddr,
                                (unsigned_char_t**) &endpoint,
                                &dbg_status);

    if (!strncmp(endpoint, "\\pipe\\", sizeof("\\pipe\\") - 1) ||
        !strncmp(endpoint, "\\PIPE\\", sizeof("\\PIPE\\") - 1))
    {
        pipename = endpoint + sizeof("\\pipe\\") - 1;
    }
    else
    {
        serr = EINVAL;
        goto error;
    }

    serr = NtStatusToUnixErrno(
        LwRtlCStringAllocatePrintf(
            &smbpath,
            "\\npvfs\\%s",
            (char*) pipename));
    if (serr)
    {
        goto error;
    }

    filename.RootFileHandle = NULL;
    filename.IoNameOptions = 0;

    serr = NtStatusToUnixErrno(
        LwRtlWC16StringAllocateFromCString(
            &filename.FileName,
            smbpath));
    if (serr)
    {
        goto error;
    }

    while (smb->state == SMB_STATE_LISTEN)
    {
        SMB_SOCKET_UNLOCK(smb);

        serr = NtStatusToUnixErrno(
            LwNtCtxCreateNamedPipeFile(
                smb->context, /* IO context */
                NULL, /* Security token */
                &smb->np, /* NP handle */
                NULL, /* Async control */
                &status_block, /* IO status block */
                &filename, /* Filename */
                NULL, /* Security descriptor */
                NULL, /* Security QOS */
                GENERIC_READ | GENERIC_WRITE, /* Desired access mode */
                SHARE_READ | SHARE_WRITE, /* Share access mode */
                OPEN_EXISTING, /* Create disposition */
                0, /* Create options */
                0, /* Named pipe type */
                0, /* Read mode */
                0, /* Completion mode */
                smb->accept_backlog.capacity, /* Maximum instances */
                0, /* Inbound quota */
                0, /* Outbound quota */
                &default_timeout /* ??? Default timeout */
                ));
        if (serr)
        {
            SMB_SOCKET_LOCK(smb);
            goto error;
        }

        serr = NtStatusToUnixErrno(
            LwIoCtxConnectNamedPipe(
                smb->context,
                smb->np));
        if (serr)
        {
            SMB_SOCKET_LOCK(smb);
            goto error;
        }

        SMB_SOCKET_LOCK(smb);

        /* Wait for a slot to open in the accept queue */
        while (smb->accept_backlog.length == smb->accept_backlog.capacity)
        {
            if (smb->state == SMB_STATE_ERROR)
            {
                goto error;
            }

            rpc__smb_socket_wait(smb);
        }

        /* Put the handle into the accept queue */
        for (i = 0; i < smb->accept_backlog.capacity; i++)
        {
            if (smb->accept_backlog.queue[i] == NULL)
            {
                smb->accept_backlog.queue[i] = smb->np;
                smb->np = NULL;
                smb->accept_backlog.length++;
                if (write(smb->accept_backlog.selectfd[1], &c, sizeof(c)) != sizeof(c))
                {
                    serr = errno;
                    goto error;
                }
                dcethread_cond_broadcast_throw(&smb->event);
                break;
            }
        }
    }

error:

    if (serr)
    {
        rpc__smb_socket_change_state(smb, SMB_STATE_ERROR);
    }

    SMB_SOCKET_UNLOCK(smb);

    return NULL;
}


INTERNAL
rpc_socket_error_t
rpc__smb_socket_listen(
    rpc_socket_t sock,
    int backlog
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    SMB_SOCKET_LOCK(smb);

    smb->accept_backlog.capacity = backlog;
    smb->accept_backlog.length = 0;
    smb->accept_backlog.queue = calloc(backlog, sizeof(*smb->accept_backlog.queue));

    if (!smb->accept_backlog.queue)
    {
        serr = ENOMEM;
        goto error;
    }

    if (pipe(smb->accept_backlog.selectfd) != 0)
    {
        serr = errno;
        goto error;
    }

    smb->state = SMB_STATE_LISTEN;

    dcethread_create_throw(&smb->listen_thread, NULL, rpc__smb_socket_listen_thread, smb);

error:

    SMB_SOCKET_UNLOCK(smb);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_do_send(
    rpc_socket_t sock
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    DWORD bytes_written = 0;
    unsigned char* cursor = smb->sendbuffer.base;
    IO_STATUS_BLOCK io_status;

    do
    {
        serr = NtStatusToUnixErrno(
            NtCtxWriteFile(
                smb->context,                          /* IO context */
                smb->np,                               /* File handle */
                NULL,                                  /* Async control block */
                &io_status,                            /* IO status block */
                smb->sendbuffer.base,                  /* Buffer */
                smb->sendbuffer.start_cursor - cursor, /* Length */
                NULL,                                  /* Byte offset */
                NULL                                   /* Key */
                ));
        if (serr)
        {
            goto error;
        }

        bytes_written = io_status.BytesTransferred;
        cursor += bytes_written;
    } while (cursor < smb->sendbuffer.start_cursor);

    /* Settle the remaining data (which hopefully should be zero if
       the runtime calls us with complete packets) to the start of
       the send buffer */
    rpc__smb_buffer_settle(&smb->sendbuffer);

error:

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_do_recv(
    rpc_socket_t sock
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    DWORD bytes_requested = 0;
    DWORD bytes_read = 0;
    IO_STATUS_BLOCK io_status;

    do
    {
        /* FIXME: magic number */
        serr = rpc__smb_buffer_ensure_available(&smb->recvbuffer, 8192);
        if (serr)
        {
            goto error;
        }

        bytes_read = 0;
        bytes_requested = rpc__smb_buffer_available(&smb->recvbuffer);

        serr = NtStatusToUnixErrno(
            NtCtxReadFile(
                smb->context,                 /* IO context */
                smb->np,                      /* File handle */
                NULL,                         /* Async control block */
                &io_status,                   /* IO status block */
                smb->recvbuffer.end_cursor,   /* Buffer */
                bytes_requested,              /* Length */
                NULL,                         /* Byte offset */
                NULL                          /* Key */
                ));
        if (serr)
        {
            goto error;
        }

        bytes_read = io_status.BytesTransferred;
        smb->recvbuffer.end_cursor += bytes_read;
    } while (bytes_read == bytes_requested);

error:

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_sendmsg(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,
    int iov_len,
    rpc_addr_p_t addr ATTRIBUTE_UNUSED,
    int *cc
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    int i;
    size_t pending = 0;

    SMB_SOCKET_LOCK(smb);

    /* Wait until we are in a state where we can send */
    while (smb->state != SMB_STATE_SEND)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            serr = -1;
            goto error;
        }
        rpc__smb_socket_wait(smb);
    }

    *cc = 0;

    /* Append all fragments into a single buffer */
    for (i = 0; i < iov_len; i++)
    {
        serr = rpc__smb_buffer_append(&smb->sendbuffer, iov[i].iov_base, iov[i].iov_len);

        if (serr)
        {
            goto error;
        }

        *cc += iov[i].iov_len;
    }

    /* Look for the last fragment and do send if we find it */
    if (rpc__smb_buffer_advance_cursor(&smb->sendbuffer, &pending))
    {
        serr = rpc__smb_socket_do_send(sock);
        if (serr)
        {
            goto error;
        }

        /* Switch into recv mode */
        rpc__smb_socket_change_state(smb, SMB_STATE_RECV);
    }

cleanup:

    SMB_SOCKET_UNLOCK(smb);

    return serr;

error:

    rpc__smb_socket_change_state(smb, SMB_STATE_ERROR);

    goto cleanup;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_recvfrom(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    byte_p_t buf ATTRIBUTE_UNUSED,
    int len ATTRIBUTE_UNUSED,
    rpc_addr_p_t from ATTRIBUTE_UNUSED,
    int *cc ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t serr = ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_recvmsg(
    rpc_socket_t sock,
    rpc_socket_iovec_p_t iov,
    int iov_len,
    rpc_addr_p_t addr,
    int *cc
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    int i;
    size_t pending;

    SMB_SOCKET_LOCK(smb);

    while (smb->state != SMB_STATE_RECV)
    {
        if (smb->state == SMB_STATE_ERROR)
        {
            serr = -1;
            goto error;
        }
        rpc__smb_socket_wait(smb);
    }

    *cc = 0;

    /* Read data until we have something in the buffer */
    while (rpc__smb_buffer_pending(&smb->recvbuffer) == 0)
    {
        serr = rpc__smb_socket_do_recv(sock);
        if (serr)
        {
            goto error;
        }
    }

    for (i = 0; i < iov_len; i++)
    {
        pending = rpc__smb_buffer_pending(&smb->recvbuffer);
        if (iov[i].iov_len < pending)
        {
            memcpy(iov[i].iov_base, smb->recvbuffer.start_cursor, iov[i].iov_len);

            smb->recvbuffer.start_cursor += iov[i].iov_len;
            *cc += iov[i].iov_len;
        }
        else
        {
            memcpy(iov[i].iov_base, smb->recvbuffer.start_cursor, pending);

            *cc += pending;

            /* Reset buffer because we have emptied it */
            smb->recvbuffer.start_cursor = smb->recvbuffer.end_cursor = smb->recvbuffer.base;
            /* Switch into send mode */
            rpc__smb_socket_change_state(smb, SMB_STATE_SEND);
        }
    }

    if (addr)
    {
        memcpy(addr, &smb->peeraddr, sizeof(smb->peeraddr));
    }

cleanup:

    SMB_SOCKET_UNLOCK(smb);

    return serr;

error:

    rpc__smb_socket_change_state(smb, SMB_STATE_ERROR);

    goto cleanup;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_inq_endpoint(
    rpc_socket_t sock,
    rpc_addr_p_t addr
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_np_addr_p_t npaddr = (rpc_np_addr_p_t) addr;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    *npaddr = smb->localaddr;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_broadcast(
    rpc_socket_t sock ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_bufs(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    unsigned32 txsize ATTRIBUTE_UNUSED,
    unsigned32 rxsize ATTRIBUTE_UNUSED,
    unsigned32 *ntxsize ATTRIBUTE_UNUSED,
    unsigned32 *nrxsize ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_nbio(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_close_on_exec(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_getpeername(
    rpc_socket_t sock,
    rpc_addr_p_t addr
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;

    SMB_SOCKET_LOCK(smb);

    if (!smb->np)
    {
        serr = EINVAL;
        goto error;
    }

    memcpy(addr, &smb->peeraddr, sizeof(smb->peeraddr));

error:

    SMB_SOCKET_UNLOCK(smb);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_get_if_id(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    rpc_network_if_id_t *network_if_id ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_keepalive(
    rpc_socket_t sock ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_nowriteblock_wait(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    struct timeval *tmo ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_set_rcvtimeo(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    struct timeval *tmo ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = RPC_C_SOCKET_OK;

    return serr;
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_getpeereid(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    uid_t *euid ATTRIBUTE_UNUSED,
    gid_t *egid ATTRIBUTE_UNUSED
    )
{
    rpc_socket_error_t serr = ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

INTERNAL
int
rpc__smb_socket_get_select_desc(
    rpc_socket_t sock
    )
{
    rpc_smb_socket_p_t smb = (rpc_smb_socket_p_t) sock->data.pointer;
    return smb->accept_backlog.selectfd[0];
}

INTERNAL
rpc_socket_error_t
rpc__smb_socket_enum_ifaces(
    rpc_socket_t sock ATTRIBUTE_UNUSED,
    rpc_socket_enum_iface_fn_p_t efun ATTRIBUTE_UNUSED,
    rpc_addr_vector_p_t *rpc_addr_vec ATTRIBUTE_UNUSED,
    rpc_addr_vector_p_t *netmask_addr_vec ATTRIBUTE_UNUSED,
    rpc_addr_vector_p_t *broadcast_addr_vec ATTRIBUTE_UNUSED
)
{
    rpc_socket_error_t serr = ENOTSUP;

    fprintf(stderr, "WARNING: unsupported smb socket function %s\n", __FUNCTION__);

    return serr;
}

rpc_socket_vtbl_t rpc_g_smb_socket_vtbl =
{
    .socket_construct = rpc__smb_socket_construct,
    .socket_destruct = rpc__smb_socket_destruct,
    .socket_bind = rpc__smb_socket_bind,
    .socket_connect = rpc__smb_socket_connect,
    .socket_accept = rpc__smb_socket_accept,
    .socket_listen = rpc__smb_socket_listen,
    .socket_sendmsg = rpc__smb_socket_sendmsg,
    .socket_recvfrom = rpc__smb_socket_recvfrom,
    .socket_recvmsg = rpc__smb_socket_recvmsg,
    .socket_inq_endpoint = rpc__smb_socket_inq_endpoint,
    .socket_set_broadcast = rpc__smb_socket_set_broadcast,
    .socket_set_bufs = rpc__smb_socket_set_bufs,
    .socket_set_nbio = rpc__smb_socket_set_nbio,
    .socket_set_close_on_exec = rpc__smb_socket_set_close_on_exec,
    .socket_getpeername = rpc__smb_socket_getpeername,
    .socket_get_if_id = rpc__smb_socket_get_if_id,
    .socket_set_keepalive = rpc__smb_socket_set_keepalive,
    .socket_nowriteblock_wait = rpc__smb_socket_nowriteblock_wait,
    .socket_set_rcvtimeo = rpc__smb_socket_set_rcvtimeo,
    .socket_getpeereid = rpc__smb_socket_getpeereid,
    .socket_get_select_desc = rpc__smb_socket_get_select_desc,
    .socket_enum_ifaces = rpc__smb_socket_enum_ifaces
};
