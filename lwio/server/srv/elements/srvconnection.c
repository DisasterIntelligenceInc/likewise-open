/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */
/*
 * Copyright Likewise Software
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.  You should have received a copy of the GNU General
 * Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */



/*
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        srvconnection.c
 *
 * Abstract:
 *
 *        Likewise IO (LWIO) - SRV
 *
 *        Elements
 *
 *        Connection Object
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 */

#include "includes.h"

static
NTSTATUS
SrvConnection2AcquireAsyncId_inlock(
   PLWIO_SRV_CONNECTION pConnection,
   PULONG64             pullAsyncId
   );

static
VOID
SrvConnectionFree(
    PLWIO_SRV_CONNECTION pConnection
    );

// Rules:
//
// Only one reader thread can read from this socket
// Multiple writers can write to the socket, in a synchronized manner per message
// Both readers and writers can set the connection state to be invalid
// The file descriptor is set to -1 only by the reader thread (not the writers)
// The file descriptor is closed only when the connection object is freed.

static
NTSTATUS
SrvConnectionAcquireSessionId_inlock(
   PLWIO_SRV_CONNECTION pConnection,
   PUSHORT             pUid
   );

static
VOID
SrvConnectionFreeContentsClientProperties(
    PSRV_CLIENT_PROPERTIES pProperties
    );

static
int
SrvConnectionSessionCompare(
    PVOID pKey1,
    PVOID pKey2
    );

static
VOID
SrvConnectionSessionRelease(
    PVOID pSession
    );

static
NTSTATUS
SrvConnection2AcquireSessionId_inlock(
   PLWIO_SRV_CONNECTION pConnection,
   PULONG64             pUid
   );

static
int
SrvConnection2SessionCompare(
    PVOID pKey1,
    PVOID pKey2
    );

static
VOID
SrvConnection2SessionRelease(
    PVOID pSession
    );

static
int
SrvConnection2AsyncStateCompare(
    PVOID pKey1,
    PVOID pKey2
    );

static
VOID
SrvConnection2AsyncStateRelease(
    PVOID pAsyncState
    );

NTSTATUS
SrvConnectionCreate(
    HANDLE                          hSocket,
    HANDLE                          hPacketAllocator,
    HANDLE                          hGssContext,
    PLWIO_SRV_SHARE_ENTRY_LIST      pShareList,
    PSRV_PROPERTIES                 pServerProperties,
    PSRV_HOST_INFO                  pHostinfo,
    PFN_LWIO_SRV_FREE_SOCKET_HANDLE pfnSocketFree,
    PLWIO_SRV_CONNECTION*           ppConnection
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_CONNECTION pConnection = NULL;

    ntStatus = SrvAllocateMemory(
                    sizeof(LWIO_SRV_CONNECTION),
                    (PVOID*)&pConnection);
    BAIL_ON_NT_STATUS(ntStatus);

    pConnection->refCount = 1;

    pthread_rwlock_init(&pConnection->mutex, NULL);
    pConnection->pMutex = &pConnection->mutex;

    pthread_mutex_init(&pConnection->mutexGssNegotiate, NULL);
    pConnection->pMutexGssNegotiate = &pConnection->mutexGssNegotiate;

    ntStatus = LwRtlRBTreeCreate(
                    &SrvConnectionSessionCompare,
                    NULL,
                    &SrvConnectionSessionRelease,
                    &pConnection->pSessionCollection);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlRBTreeCreate(
                    &SrvConnection2AsyncStateCompare,
                    NULL,
                    &SrvConnection2AsyncStateRelease,
                    &pConnection->pAsyncStateCollection);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvAcquireHostInfo(
                    pHostinfo,
                    &pConnection->pHostinfo);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvGssAcquireContext(
                    pConnection->pHostinfo,
                    hGssContext,
                    &pConnection->hGssContext);
    BAIL_ON_NT_STATUS(ntStatus);

    pConnection->ulSequence = 0;
    pConnection->hPacketAllocator = hPacketAllocator;
    pConnection->pShareList = pShareList;
    pConnection->state = LWIO_SRV_CONN_STATE_INITIAL;
    pConnection->hSocket = hSocket;
    pConnection->pfnSocketFree = pfnSocketFree;

    memcpy(&pConnection->serverProperties, pServerProperties, sizeof(*pServerProperties));
    uuid_copy(pConnection->serverProperties.GUID, pServerProperties->GUID);

    SRV_ELEMENTS_INCREMENT_CONNECTIONS;

    *ppConnection = pConnection;

cleanup:

    return ntStatus;

error:

    *ppConnection = NULL;

    if (pConnection)
    {
        SrvConnectionRelease(pConnection);
    }

    goto cleanup;
}

SMB_PROTOCOL_VERSION
SrvConnectionGetProtocolVersion(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    BOOLEAN bInLock = FALSE;
    SMB_PROTOCOL_VERSION protocolVersion = SMB_PROTOCOL_VERSION_UNKNOWN;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pConnection->mutex);

    protocolVersion = pConnection->protocolVer;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return protocolVersion;
}

NTSTATUS
SrvConnectionSetProtocolVersion(
    PLWIO_SRV_CONNECTION pConnection,
    SMB_PROTOCOL_VERSION protoVer
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN  bInLock  = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    ntStatus = SrvConnectionSetProtocolVersion_inlock(pConnection, protoVer);

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;
}

NTSTATUS
SrvConnectionSetProtocolVersion_inlock(
    PLWIO_SRV_CONNECTION pConnection,
    SMB_PROTOCOL_VERSION protoVer
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (protoVer != pConnection->protocolVer)
    {
        if (pConnection->pSessionCollection)
        {
            LwRtlRBTreeFree(pConnection->pSessionCollection);
            pConnection->pSessionCollection = NULL;
        }

        pConnection->protocolVer = protoVer;

        switch (protoVer)
        {
            case SMB_PROTOCOL_VERSION_1:

                pConnection->ulSequence = 0;
                pConnection->usNextAvailableUid = 0;

                ntStatus = LwRtlRBTreeCreate(
                                &SrvConnectionSessionCompare,
                                NULL,
                                &SrvConnectionSessionRelease,
                                &pConnection->pSessionCollection);

                break;

            case SMB_PROTOCOL_VERSION_2:

                pConnection->ullNextAvailableUid = 0;

                ntStatus = LwRtlRBTreeCreate(
                                &SrvConnection2SessionCompare,
                                NULL,
                                &SrvConnection2SessionRelease,
                                &pConnection->pSessionCollection);

                break;

            default:

                ntStatus = STATUS_INVALID_PARAMETER_2;

                break;
        }
        BAIL_ON_NT_STATUS(ntStatus);
    }

error:

    return ntStatus;
}

BOOLEAN
SrvConnectionIsInvalid(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    BOOLEAN bInvalid = FALSE;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pConnection->mutex);

    bInvalid = pConnection->state == LWIO_SRV_CONN_STATE_INVALID;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return bInvalid;
}

VOID
SrvConnectionSetInvalid(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    pConnection->state = LWIO_SRV_CONN_STATE_INVALID;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);
}

LWIO_SRV_CONN_STATE
SrvConnectionGetState(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    LWIO_SRV_CONN_STATE connState = LWIO_SRV_CONN_STATE_INITIAL;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pConnection->mutex);

    connState = pConnection->state;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return connState;
}

VOID
SrvConnectionSetState(
    PLWIO_SRV_CONNECTION pConnection,
    LWIO_SRV_CONN_STATE  connState
    )
{
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    pConnection->state = connState;

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);
}


NTSTATUS
SrvConnectionFindSession(
    PLWIO_SRV_CONNECTION pConnection,
    USHORT uid,
    PLWIO_SRV_SESSION* ppSession
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_SESSION pSession = NULL;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pConnection->mutex);

    pSession = pConnection->lruSession[ uid % SRV_LRU_CAPACITY ];
    if (!pSession || (pSession->uid != uid))
    {
        ntStatus = LwRtlRBTreeFind(
                        pConnection->pSessionCollection,
                        &uid,
                        (PVOID*)&pSession);
        BAIL_ON_NT_STATUS(ntStatus);

        pConnection->lruSession[ uid % SRV_LRU_CAPACITY ] = pSession;
    }

    InterlockedIncrement(&pSession->refcount);

    *ppSession = pSession;

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:
    if (ntStatus == STATUS_NOT_FOUND)
    {
        ntStatus = STATUS_INVALID_HANDLE;
    }

    *ppSession = NULL;

    goto cleanup;
}

NTSTATUS
SrvConnection2FindSession(
    PLWIO_SRV_CONNECTION pConnection,
    ULONG64              ullUid,
    PLWIO_SRV_SESSION_2* ppSession
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_SESSION_2 pSession = NULL;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pConnection->mutex);

    pSession = pConnection->lruSession2[ ullUid % SRV_LRU_CAPACITY ];
    if (!pSession || (pSession->ullUid != ullUid))
    {
        ntStatus = LwRtlRBTreeFind(
                        pConnection->pSessionCollection,
                        &ullUid,
                        (PVOID*)&pSession);
        BAIL_ON_NT_STATUS(ntStatus);

        pConnection->lruSession2[ ullUid % SRV_LRU_CAPACITY ] = pSession;
    }

    InterlockedIncrement(&pSession->refcount);

    *ppSession = pSession;

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:
    if (ntStatus == STATUS_NOT_FOUND)
    {
        ntStatus = STATUS_INVALID_HANDLE;
    }

    *ppSession = NULL;

    goto cleanup;
}

NTSTATUS
SrvConnectionRemoveSession(
    PLWIO_SRV_CONNECTION pConnection,
    USHORT              uid
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    PLWIO_SRV_SESSION pSession = NULL;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    pSession = pConnection->lruSession[ uid % SRV_LRU_CAPACITY ];
    if (pSession && (pSession->uid == uid))
    {
        pConnection->lruSession[ uid % SRV_LRU_CAPACITY ] = NULL;
    }

    ntStatus = LwRtlRBTreeRemove(
                    pConnection->pSessionCollection,
                    &uid);
    BAIL_ON_NT_STATUS(ntStatus);

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:

    goto cleanup;
}

NTSTATUS
SrvConnection2RemoveSession(
    PLWIO_SRV_CONNECTION pConnection,
    ULONG64              ullUid
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    PLWIO_SRV_SESSION_2 pSession = NULL;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    pSession = pConnection->lruSession2[ ullUid % SRV_LRU_CAPACITY ];
    if (pSession && (pSession->ullUid == ullUid))
    {
        pConnection->lruSession2[ ullUid % SRV_LRU_CAPACITY ] = NULL;
    }

    ntStatus = LwRtlRBTreeRemove(
                    pConnection->pSessionCollection,
                    &ullUid);
    BAIL_ON_NT_STATUS(ntStatus);

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:

    goto cleanup;
}

NTSTATUS
SrvConnectionCreateSession(
    PLWIO_SRV_CONNECTION pConnection,
    PLWIO_SRV_SESSION* ppSession
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_SESSION pSession = NULL;
    BOOLEAN bInLock = FALSE;
    USHORT  uid = 0;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    ntStatus = SrvConnectionAcquireSessionId_inlock(
                    pConnection,
                    &uid);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvSessionCreate(
                    uid,
                    &pSession);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlRBTreeAdd(
                    pConnection->pSessionCollection,
                    &pSession->uid,
                    pSession);
    BAIL_ON_NT_STATUS(ntStatus);

    InterlockedIncrement(&pSession->refcount);

    *ppSession = pSession;

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:

    *ppSession = NULL;

    if (pSession)
    {
        SrvSessionRelease(pSession);
    }

    goto cleanup;
}

NTSTATUS
SrvConnection2CreateSession(
    PLWIO_SRV_CONNECTION pConnection,
    PLWIO_SRV_SESSION_2* ppSession
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_SESSION_2 pSession = NULL;
    BOOLEAN bInLock = FALSE;
    ULONG64 ullUid = 0;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    ntStatus = SrvConnection2AcquireSessionId_inlock(
                    pConnection,
                    &ullUid);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvSession2Create(
                    ullUid,
                    &pSession);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlRBTreeAdd(
                    pConnection->pSessionCollection,
                    &pSession->ullUid,
                    pSession);
    BAIL_ON_NT_STATUS(ntStatus);

    InterlockedIncrement(&pSession->refcount);

    *ppSession = pSession;

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:

    *ppSession = NULL;

    if (pSession)
    {
        SrvSession2Release(pSession);
    }

    goto cleanup;
}

NTSTATUS
SrvConnection2CreateAsyncState(
    PLWIO_SRV_CONNECTION          pConnection,
    USHORT                        usCommand,
    PFN_LWIO_SRV_FREE_ASYNC_STATE pfnFreeAsyncState,
    PLWIO_ASYNC_STATE*            ppAsyncState
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN  bInLock  = FALSE;
    PLWIO_ASYNC_STATE pAsyncState = NULL;
    ULONG64           ullAsyncId  = 0;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    ntStatus = SrvConnection2AcquireAsyncId_inlock(
                    pConnection,
                    &ullAsyncId);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvAsyncStateCreate(
                    ullAsyncId,
                    usCommand,
                    NULL,
                    pfnFreeAsyncState,
                    &pAsyncState);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlRBTreeAdd(
                    pConnection->pAsyncStateCollection,
                    &pAsyncState->ullAsyncId,
                    pAsyncState);
    BAIL_ON_NT_STATUS(ntStatus);

    *ppAsyncState = SrvAsyncStateAcquire(pAsyncState);

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:

    *ppAsyncState = NULL;

    if (pAsyncState)
    {
        SrvAsyncStateRelease(pAsyncState);
    }

    goto cleanup;
}

static
NTSTATUS
SrvConnection2AcquireAsyncId_inlock(
   PLWIO_SRV_CONNECTION pConnection,
   PULONG64             pullAsyncId
   )
{
    NTSTATUS ntStatus = 0;
    ULONG64  ullCandidateAsyncId = pConnection->ullNextAvailableAsyncId;
    BOOLEAN  bFound = FALSE;

    do
    {
        PLWIO_ASYNC_STATE pAsyncState = NULL;

        /* 0 is never a valid async id */

        if ((ullCandidateAsyncId == 0) || (ullCandidateAsyncId == UINT64_MAX))
        {
            ullCandidateAsyncId = 1;
        }

        ntStatus = LwRtlRBTreeFind(
                        pConnection->pAsyncStateCollection,
                        &ullCandidateAsyncId,
                        (PVOID*)&pAsyncState);
        if (ntStatus == STATUS_NOT_FOUND)
        {
            ntStatus = STATUS_SUCCESS;
            bFound = TRUE;
        }
        else
        {
            ullCandidateAsyncId++;
        }
        BAIL_ON_NT_STATUS(ntStatus);

    } while ((ullCandidateAsyncId != pConnection->ullNextAvailableAsyncId) && !bFound);

    if (!bFound)
    {
        ntStatus = STATUS_TOO_MANY_CONTEXT_IDS;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    *pullAsyncId = ullCandidateAsyncId;

    /* Increment by 1 by make sure to deal with wrap around */

    ullCandidateAsyncId++;
    pConnection->ullNextAvailableAsyncId = ullCandidateAsyncId ? ullCandidateAsyncId : 1;

cleanup:

    return ntStatus;

error:

    *pullAsyncId = 0;

    goto cleanup;
}

NTSTATUS
SrvConnection2FindAsyncState(
    PLWIO_SRV_CONNECTION pConnection,
    ULONG64              ullAsyncId,
    PLWIO_ASYNC_STATE*   ppAsyncState
    )
{
    NTSTATUS          ntStatus = STATUS_SUCCESS;
    PLWIO_ASYNC_STATE pAsyncState = NULL;
    BOOLEAN           bInLock     = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pConnection->mutex);

    ntStatus = LwRtlRBTreeFind(
                    pConnection->pAsyncStateCollection,
                    &ullAsyncId,
                    (PVOID*)&pAsyncState);
    BAIL_ON_NT_STATUS(ntStatus);

    *ppAsyncState = SrvAsyncStateAcquire(pAsyncState);

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;

error:

    *ppAsyncState = NULL;

    goto cleanup;
}

NTSTATUS
SrvConnection2RemoveAsyncState(
    PLWIO_SRV_CONNECTION pConnection,
    ULONG64              ullAsyncId
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN  bInLock  = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pConnection->mutex);

    ntStatus = LwRtlRBTreeRemove(
                    pConnection->pAsyncStateCollection,
                    &ullAsyncId);

    LWIO_UNLOCK_RWMUTEX(bInLock, &pConnection->mutex);

    return ntStatus;
}

NTSTATUS
SrvConnectionGetNamedPipeSessionKey(
    PLWIO_SRV_CONNECTION pConnection,
    PIO_ECP_LIST        pEcpList
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PBYTE pSessionKey = pConnection->pSessionKey;
    ULONG ulSessionKeyLength = pConnection->ulSessionKeyLength;

    if (pSessionKey != NULL)
    {
        ntStatus = IoRtlEcpListInsert(pEcpList,
                                      IO_ECP_TYPE_SESSION_KEY,
                                      pSessionKey,
                                      ulSessionKeyLength,
                                      NULL);
        BAIL_ON_NT_STATUS(ntStatus);
    }

cleanup:

    return ntStatus;

error:

    goto cleanup;
}

PLWIO_SRV_CONNECTION
SrvConnectionAcquire(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    InterlockedIncrement(&pConnection->refCount);

    return pConnection;
}

VOID
SrvConnectionRelease(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    if (InterlockedDecrement(&pConnection->refCount) == 0)
    {
        SRV_ELEMENTS_DECREMENT_CONNECTIONS;

        SrvConnectionFree(pConnection);
    }
}

static
VOID
SrvConnectionFree(
    PLWIO_SRV_CONNECTION pConnection
    )
{
    if (pConnection->readerState.pRequestPacket)
    {
        SMBPacketRelease(
            pConnection->hPacketAllocator,
            pConnection->readerState.pRequestPacket);
    }

    if (pConnection->pSessionKey)
    {
        SrvFreeMemory(pConnection->pSessionKey);
    }

    if (pConnection->hGssNegotiate)
    {
        SrvGssEndNegotiate(
            pConnection->hGssContext,
            pConnection->hGssNegotiate);
    }

    if (pConnection->hGssContext)
    {
        SrvGssReleaseContext(pConnection->hGssContext);
    }

    if (pConnection->hSocket && pConnection->pfnSocketFree)
    {
        pConnection->pfnSocketFree(pConnection->hSocket);
    }

    if (pConnection->pSessionCollection)
    {
        LwRtlRBTreeFree(pConnection->pSessionCollection);
    }

    if (pConnection->pAsyncStateCollection)
    {
        LwRtlRBTreeFree(pConnection->pAsyncStateCollection);
    }

    if (pConnection->pHostinfo)
    {
        SrvReleaseHostInfo(pConnection->pHostinfo);
    }

    if (pConnection->pMutex)
    {
        pthread_rwlock_destroy(&pConnection->mutex);
        pConnection->pMutex = NULL;
    }

    if (pConnection->pMutexGssNegotiate)
    {
        pthread_mutex_destroy(&pConnection->mutexGssNegotiate);;
        pConnection->pMutexGssNegotiate = NULL;
    }

    SrvConnectionFreeContentsClientProperties(&pConnection->clientProperties);

    SrvFreeMemory(pConnection);
}

static
NTSTATUS
SrvConnectionAcquireSessionId_inlock(
   PLWIO_SRV_CONNECTION pConnection,
   PUSHORT             pUid
   )
{
    NTSTATUS ntStatus = 0;
    USHORT   candidateUid = pConnection->usNextAvailableUid;
    BOOLEAN  bFound = FALSE;

    do
    {
        PLWIO_SRV_SESSION pSession = NULL;

        /* 0 is never a valid session vuid */

        if ((candidateUid == 0) || (candidateUid == UINT16_MAX))
        {
            candidateUid = 1;
        }

        ntStatus = LwRtlRBTreeFind(
                        pConnection->pSessionCollection,
                        &candidateUid,
                        (PVOID*)&pSession);
        if (ntStatus == STATUS_NOT_FOUND)
        {
            ntStatus = STATUS_SUCCESS;
            bFound = TRUE;
        }
        else
        {
            candidateUid++;
        }
        BAIL_ON_NT_STATUS(ntStatus);

    } while ((candidateUid != pConnection->usNextAvailableUid) && !bFound);

    if (!bFound)
    {
        ntStatus = STATUS_TOO_MANY_SESSIONS;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    *pUid = candidateUid;

    /* Increment by 1 by make sure tyo deal with wraparound */

    candidateUid++;
    pConnection->usNextAvailableUid = candidateUid ? candidateUid : 1;

cleanup:

    return ntStatus;

error:

    *pUid = 0;

    goto cleanup;
}

static
VOID
SrvConnectionFreeContentsClientProperties(
    PSRV_CLIENT_PROPERTIES pProperties
    )
{
    if (pProperties->pwszNativeLanMan)
    {
        SrvFreeMemory(pProperties->pwszNativeLanMan);
    }
    if (pProperties->pwszNativeOS)
    {
        SrvFreeMemory(pProperties->pwszNativeOS);
    }
    if (pProperties->pwszNativeDomain)
    {
        SrvFreeMemory(pProperties->pwszNativeDomain);
    }
}

static
int
SrvConnectionSessionCompare(
    PVOID pKey1,
    PVOID pKey2
    )
{
    PUSHORT pUid1 = (PUSHORT)pKey1;
    PUSHORT pUid2 = (PUSHORT)pKey2;

    assert (pUid1 != NULL);
    assert (pUid2 != NULL);

    if (*pUid1 > *pUid2)
    {
        return 1;
    }
    else if (*pUid1 < *pUid2)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static
VOID
SrvConnectionSessionRelease(
    PVOID pSession
    )
{
    SrvSessionRelease((PLWIO_SRV_SESSION)pSession);
}

static
NTSTATUS
SrvConnection2AcquireSessionId_inlock(
   PLWIO_SRV_CONNECTION pConnection,
   PULONG64             pUid
   )
{
    NTSTATUS ntStatus = 0;
    ULONG64  candidateUid = pConnection->ullNextAvailableUid;
    BOOLEAN  bFound = FALSE;

    do
    {
        PLWIO_SRV_SESSION_2 pSession = NULL;

        /* 0 is never a valid session vuid */

        if ((candidateUid == 0) || (candidateUid == UINT64_MAX))
        {
            candidateUid = 1;
        }

        ntStatus = LwRtlRBTreeFind(
                        pConnection->pSessionCollection,
                        &candidateUid,
                        (PVOID*)&pSession);
        if (ntStatus == STATUS_NOT_FOUND)
        {
            ntStatus = STATUS_SUCCESS;
            bFound = TRUE;
        }
        else
        {
            candidateUid++;
        }
        BAIL_ON_NT_STATUS(ntStatus);

    } while ((candidateUid != pConnection->ullNextAvailableUid) && !bFound);

    if (!bFound)
    {
        ntStatus = STATUS_TOO_MANY_SESSIONS;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    *pUid = candidateUid;

    /* Increment by 1 by make sure to deal with wrap-around */

    candidateUid++;
    pConnection->ullNextAvailableUid = candidateUid ? candidateUid : 1;

cleanup:

    return ntStatus;

error:

    *pUid = 0;

    goto cleanup;
}

static
int
SrvConnection2SessionCompare(
    PVOID pKey1,
    PVOID pKey2
    )
{
    PULONG64 pUid1 = (PULONG64)pKey1;
    PULONG64 pUid2 = (PULONG64)pKey2;

    assert (pUid1 != NULL);
    assert (pUid2 != NULL);

    if (*pUid1 > *pUid2)
    {
        return 1;
    }
    else if (*pUid1 < *pUid2)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static
VOID
SrvConnection2SessionRelease(
    PVOID pSession
    )
{
    SrvSession2Release((PLWIO_SRV_SESSION_2)pSession);
}

static
int
SrvConnection2AsyncStateCompare(
    PVOID pKey1,
    PVOID pKey2
    )
{
    PULONG64 pAsyncId1 = (PULONG64)pKey1;
    PULONG64 pAsyncId2 = (PULONG64)pKey2;

    if (*pAsyncId1 > *pAsyncId2)
    {
        return 1;
    }
    else if (*pAsyncId1 < *pAsyncId2)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static
VOID
SrvConnection2AsyncStateRelease(
    PVOID pAsyncState
    )
{
    SrvAsyncStateRelease((PLWIO_ASYNC_STATE)pAsyncState);
}



