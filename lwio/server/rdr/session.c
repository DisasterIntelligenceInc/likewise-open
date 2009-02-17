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
 *        session.c
 *
 * Abstract:
 *
 *        Likewise SMB Subsystem (LWIO)
 *
 *        Common Session Code
 *
 * Author: Kaya Bekiroglu (kaya@likewisesoftware.com)
 *
 * @todo: add error logging code
 * @todo: switch to NT error codes where appropriate
 */

#include "rdr.h"

static
int
SMBSessionHashTreeCompareByTID(
    PCVOID vp1,
    PCVOID vp2
    );

static
size_t
SMBSessionHashTreeByTID(
    PCVOID vp
    );

static
VOID
SMBSessionFree(
    PSMB_SESSION pSession
    );

NTSTATUS
SMBSessionCreate(
    PSMB_SESSION* ppSession
    )
{
    NTSTATUS ntStatus = 0;
    SMB_SESSION *pSession = NULL;
    BOOLEAN bDestroyCondition = FALSE;
    BOOLEAN bDestroyHashLock = FALSE;
    BOOLEAN bDestroySetupCondition = FALSE;
    BOOLEAN bDestroyMutex = FALSE;
    BOOLEAN bDestroyTreeMutex = FALSE;

    ntStatus = SMBAllocateMemory(
                sizeof(SMB_SESSION),
                (PVOID*)&pSession);
    BAIL_ON_NT_STATUS(ntStatus);

    pthread_mutex_init(&pSession->mutex, NULL);
    bDestroyMutex = TRUE;

    pSession->state = SMB_RESOURCE_STATE_INITIALIZING;
    pSession->error.type = ERROR_SMB;
    pSession->error.smb = SMB_ERROR_SUCCESS;

    ntStatus = pthread_cond_init(&pSession->event, NULL);
    BAIL_ON_NT_STATUS(ntStatus);

    bDestroyCondition = TRUE;

    ntStatus = pthread_rwlock_init(&pSession->hashLock, NULL);
    BAIL_ON_NT_STATUS(ntStatus);

    bDestroyHashLock = TRUE;

    pSession->refCount = 2; /* One for reaper */

    /* @todo: find a portable time call which is immune to host date and time
       changes, such as made by ntpd */
    ntStatus = time(&pSession->lastActiveTime);
    if (ntStatus == -1)
    {
        ntStatus = errno;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    ntStatus = SMBHashCreate(
                19,
                SMBHashCaselessStringCompare,
                SMBHashCaselessString,
                NULL,
                &pSession->pTreeHashByPath);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SMBHashCreate(
                19,
                &SMBSessionHashTreeCompareByTID,
                &SMBSessionHashTreeByTID,
                NULL,
                &pSession->pTreeHashByTID);
    BAIL_ON_NT_STATUS(ntStatus);

    pthread_mutex_init(&pSession->treeMutex, NULL);
    bDestroyTreeMutex = TRUE;

    pSession->pTreePacket = NULL;

    ntStatus = pthread_cond_init(&pSession->treeEvent, NULL);
    BAIL_ON_NT_STATUS(ntStatus);

    bDestroySetupCondition = TRUE;

    *ppSession = pSession;

cleanup:

    return ntStatus;

error:

    if (pSession)
    {
        SMBHashSafeFree(&pSession->pTreeHashByTID);

        SMBHashSafeFree(&pSession->pTreeHashByPath);

        if (bDestroyCondition)
        {
            pthread_cond_destroy(&pSession->event);
        }

        if (bDestroyHashLock)
        {
            pthread_rwlock_destroy(&pSession->hashLock);
        }

        if (bDestroyMutex)
        {
            pthread_mutex_destroy(&pSession->mutex);
        }

        if (bDestroySetupCondition)
        {
            pthread_cond_destroy(&pSession->treeEvent);
        }

        if (bDestroyTreeMutex)
        {
            pthread_mutex_destroy(&pSession->treeMutex);
        }

        SMBFreeMemory(pSession);
    }

    *ppSession = NULL;

    goto cleanup;
}

static
int
SMBSessionHashTreeCompareByTID(
    PCVOID vp1,
    PCVOID vp2
    )
{
    uint16_t tid1 = *((uint16_t *) vp1);
    uint16_t tid2 = *((uint16_t *) vp2);

    if (tid1 == tid2)
    {
        return 0;
    }
    else if (tid1 > tid2)
    {
        return 1;
    }

    return -1;
}

static
size_t
SMBSessionHashTreeByTID(
    PCVOID vp
    )
{
    return *((uint16_t *) vp);
}

/* This must be called with the parent hash lock held to avoid a race with the
   reaper. */
VOID
SMBSessionAddReference(
    PSMB_SESSION pSession
    )
{
    BOOLEAN bInLock = FALSE;

    SMB_LOCK_MUTEX(bInLock, &pSession->mutex);

    pSession->refCount++;

    SMB_UNLOCK_MUTEX(bInLock, &pSession->mutex);
}

/** @todo: keep unused trees around for a little while when daemonized.
  * To avoid writing frequently to shared cache lines, perhaps set a bit
  * when the hash transitions to non-empty, then periodically sweep for
  * empty hashes.  If a hash is empty after x number of timed sweeps, tear
  * down the parent.
  */
/* This function does not decrement the reference count of the parent
   socket on destruction.  The reaper thread manages that with proper forward
   locking. */
VOID
SMBSessionRelease(
    PSMB_SESSION pSession
    )
{
    BOOLEAN bInLock = FALSE;

    SMB_LOCK_MUTEX(bInLock, &pSession->mutex);

    pSession->refCount--;
    assert(pSession->refCount >= 0);

    if (!pSession->refCount)
    {
        SMBSessionFree(pSession);

        bInLock = FALSE;
    }

    SMB_UNLOCK_MUTEX(bInLock, &pSession->mutex);
}

/* This function does not remove a socket from it's parent hash; it merely
   frees the memory if the refcount is zero. */
static
VOID
SMBSessionFree(
    PSMB_SESSION pSession
    )
{
    assert(!pSession->refCount);

    pthread_cond_destroy(&pSession->event);

    /* @todo: assert that the session hashes are empty */
    SMBHashSafeFree(&pSession->pTreeHashByPath);
    SMBHashSafeFree(&pSession->pTreeHashByTID);

    assert(!pSession->pTreePacket);

    pthread_cond_destroy(&pSession->treeEvent);

    pthread_mutex_destroy(&pSession->treeMutex);
    pthread_mutex_destroy(&pSession->mutex);
    pthread_rwlock_destroy(&pSession->hashLock);

    SMB_SAFE_FREE_MEMORY(pSession->pSessionKey);

    /* @todo: use allocator */
    SMBFreeMemory(pSession);
}

VOID
SMBSessionInvalidate(
    PSMB_SESSION   pSession,
    SMB_ERROR_TYPE errorType,
    uint32_t       networkError
    )
{
    BOOLEAN bInLock = FALSE;

    SMB_LOCK_MUTEX(bInLock, &pSession->mutex);

    /* mark permanent error, continue */
    /* @todo: convert socket errors into NT errors */
    pSession->error.type = errorType;
    pSession->error.smb  = networkError;
    /* Permanent error?  Or temporary? */
    /* If temporary, retry? */
    pSession->state = SMB_RESOURCE_STATE_INVALID;

    pthread_cond_broadcast(&pSession->event);

    pthread_cond_broadcast(&pSession->treeEvent);

    SMB_UNLOCK_MUTEX(bInLock, &pSession->mutex);
}

VOID
SMBSessionSetState(
    PSMB_SESSION pSession,
    SMB_RESOURCE_STATE state
    )
{
    BOOLEAN bInLock = FALSE;

    SMB_LOCK_MUTEX(bInLock, &pSession->mutex);

    pSession->state = state;

    pthread_cond_broadcast(&pSession->event);

    SMB_UNLOCK_MUTEX(bInLock, &pSession->mutex);
}

NTSTATUS
SMBSessionFindTreeByPath(
    IN PSMB_SESSION pSession,
    IN PCSTR pszPath,
    OUT PSMB_TREE* ppTree
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    PSMB_TREE pTree = NULL;

    SMB_LOCK_RWMUTEX_SHARED(bInLock, &pSession->hashLock);

    ntStatus = SMBHashGetValue(
                pSession->pTreeHashByPath,
                pszPath,
                (PVOID *) &pTree);
    BAIL_ON_NT_STATUS(ntStatus);

    SMBTreeAddReference(pTree);

    *ppTree = pTree;

cleanup:

    SMB_UNLOCK_RWMUTEX(bInLock, &pSession->hashLock);

    return ntStatus;

error:

    *ppTree = NULL;

    goto cleanup;
}

NTSTATUS
SMBSessionFindTreeById(
    PSMB_SESSION pSession,
    uint16_t     tid,
    PSMB_TREE*   ppTree
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    PSMB_TREE pTree = NULL;

    SMB_LOCK_RWMUTEX_SHARED(bInLock, &pSession->hashLock);

    ntStatus = SMBHashGetValue(
                    pSession->pTreeHashByTID,
                    &tid,
                    (PVOID *) &pTree);
    BAIL_ON_NT_STATUS(ntStatus);

    SMBTreeAddReference(pTree);

    *ppTree = pTree;

cleanup:

    SMB_UNLOCK_RWMUTEX(bInLock, &pSession->hashLock);

    return ntStatus;

error:

    *ppTree = NULL;

    goto cleanup;
}

NTSTATUS
SMBSessionReceiveResponse(
    IN PSMB_SESSION pSession,
    IN BOOLEAN bVerifySignature,
    IN DWORD dwExpectedSequence,
    OUT PSMB_PACKET* ppPacket
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    struct timespec ts = { 0, 0 };
    PSMB_PACKET pPacket = NULL;

    // TODO-The pSocket->pTreePacket stuff needs to go away
    // so that this function can go away.

    SMB_LOCK_MUTEX(bInLock, &pSession->mutex);

    while (!pSession->pTreePacket)
    {
        ts.tv_sec = time(NULL) + 30;
        ts.tv_nsec = 0;

retry_wait:

        /* @todo: always verify non-error state after acquiring mutex */
        ntStatus = pthread_cond_timedwait(
                        &pSession->treeEvent,
                        &pSession->mutex,
                        &ts);
        if (ntStatus == ETIMEDOUT)
        {
            if (time(NULL) < ts.tv_sec)
            {
                ntStatus = 0;
                goto retry_wait;
            }

            /* As long as the socket is active, continue to wait.
             * otherwise, mark the socket as bad and return
             */
            if (SMBSocketTimedOut(pSession->pSocket))
            {
                SMBSocketInvalidate(pSession->pSocket, ERROR_SMB, ETIMEDOUT);
                ntStatus = ETIMEDOUT;
            }
            else
            {
                ntStatus = SMB_ERROR_SUCCESS;
            }
        }
        BAIL_ON_NT_STATUS(ntStatus);
    }

    pPacket = pSession->pTreePacket;
    pSession->pTreePacket = NULL;

    ntStatus = SMBPacketDecodeHeader(
                    pPacket,
                    bVerifySignature,
                    dwExpectedSequence,
                    pSession->pSocket->pSessionKey,
                    pSession->pSocket->dwSessionKeyLength);
    BAIL_ON_NT_STATUS(ntStatus);

cleanup:
    SMB_UNLOCK_MUTEX(bInLock, &pSession->mutex);

    *ppPacket = pPacket;

    return ntStatus;

error:
    if (pPacket)
    {
        SMBPacketFree(pSession->pSocket->hPacketAllocator, pPacket);
        pPacket = NULL;
    }

    goto cleanup;
}

VOID
SMBSessionUpdateLastActiveTime(
    PSMB_SESSION pSession
    )
{
    BOOLEAN bInLock = FALSE;

    SMB_LOCK_MUTEX(bInLock, &pSession->mutex);

    pSession->lastActiveTime = time(NULL);

    SMB_UNLOCK_MUTEX(bInLock, &pSession->mutex);
}
