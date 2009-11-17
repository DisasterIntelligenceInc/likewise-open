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
 *        srvtree2.c
 *
 * Abstract:
 *
 *        Likewise IO (LWIO) - SRV
 *
 *        Elements
 *
 *        Tree Object (Version 2)
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 */

#include "includes.h"

static
NTSTATUS
SrvTree2AcquireFileId_inlock(
   PLWIO_SRV_TREE_2 pTree,
   PULONG64         pullFid
   );

static
int
SrvTree2FileCompare(
    PVOID pKey1,
    PVOID pKey2
    );

static
VOID
SrvTree2FileRelease(
    PVOID pFile
    );

static
VOID
SrvTree2Free(
    PLWIO_SRV_TREE_2 pTree
    );

NTSTATUS
SrvTree2Create(
    ULONG             ulTid,
    PSRV_SHARE_INFO   pShareInfo,
    PLWIO_SRV_TREE_2* ppTree
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_TREE_2 pTree = NULL;

    LWIO_LOG_DEBUG("Creating Tree [tid: %u]", ulTid);

    ntStatus = SrvAllocateMemory(sizeof(LWIO_SRV_TREE_2), (PVOID*)&pTree);
    BAIL_ON_NT_STATUS(ntStatus);

    pTree->refcount = 1;

    pthread_rwlock_init(&pTree->mutex, NULL);
    pTree->pMutex = &pTree->mutex;

    pTree->ulTid = ulTid;

    LWIO_LOG_DEBUG("Associating Tree [object:0x%x][tid:%u]",
                    pTree,
                    ulTid);

    pTree->pShareInfo = pShareInfo;
    InterlockedIncrement(&pShareInfo->refcount);

    ntStatus = LwRtlRBTreeCreate(
                    &SrvTree2FileCompare,
                    NULL,
                    &SrvTree2FileRelease,
                    &pTree->pFileCollection);
    BAIL_ON_NT_STATUS(ntStatus);

    *ppTree = pTree;

cleanup:

    return ntStatus;

error:

    *ppTree = NULL;

    if (pTree)
    {
        SrvTree2Release(pTree);
    }

    goto cleanup;
}

NTSTATUS
SrvTree2FindFile(
    PLWIO_SRV_TREE_2  pTree,
    ULONG64           ullFid,
    PLWIO_SRV_FILE_2* ppFile
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_FILE_2 pFile = NULL;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pTree->mutex);

    pFile = pTree->lruFile[ ullFid % SRV_LRU_CAPACITY ];

    if (!pFile || (pFile->ullFid != ullFid))
    {
        ntStatus = LwRtlRBTreeFind(
                        pTree->pFileCollection,
                        &ullFid,
                        (PVOID*)&pFile);
        BAIL_ON_NT_STATUS(ntStatus);

        pTree->lruFile[ullFid % SRV_LRU_CAPACITY] = pFile;
    }

    InterlockedIncrement(&pFile->refcount);

    *ppFile = pFile;

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pTree->mutex);

    return ntStatus;

error:
    if (ntStatus == STATUS_NOT_FOUND)
    {
        ntStatus = STATUS_INVALID_HANDLE;
    }

    *ppFile = NULL;

    goto cleanup;
}

NTSTATUS
SrvTree2CreateFile(
    PLWIO_SRV_TREE_2        pTree,
    PWSTR                   pwszFilename,
    PIO_FILE_HANDLE         phFile,
    PIO_FILE_NAME*          ppFilename,
    ACCESS_MASK             desiredAccess,
    LONG64                  allocationSize,
    FILE_ATTRIBUTES         fileAttributes,
    FILE_SHARE_FLAGS        shareAccess,
    FILE_CREATE_DISPOSITION createDisposition,
    FILE_CREATE_OPTIONS     createOptions,
    PLWIO_SRV_FILE_2*       ppFile
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    PLWIO_SRV_FILE_2 pFile = NULL;
    ULONG64  ullFid = 0;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pTree->mutex);

    ntStatus = SrvTree2AcquireFileId_inlock(
                    pTree,
                    &ullFid);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvFile2Create(
                    ullFid,
                    pwszFilename,
                    phFile,
                    ppFilename,
                    desiredAccess,
                    allocationSize,
                    fileAttributes,
                    shareAccess,
                    createDisposition,
                    createOptions,
                    &pFile);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlRBTreeAdd(
                    pTree->pFileCollection,
                    &pFile->ullFid,
                    pFile);
    BAIL_ON_NT_STATUS(ntStatus);

    InterlockedIncrement(&pFile->refcount);

    *ppFile = pFile;

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pTree->mutex);

    return ntStatus;

error:

    *ppFile = NULL;

    if (pFile)
    {
        SrvFile2Release(pFile);
    }

    goto cleanup;
}

NTSTATUS
SrvTree2RemoveFile(
    PLWIO_SRV_TREE_2 pTree,
    ULONG64          ullFid
    )
{
    NTSTATUS ntStatus = 0;
    BOOLEAN bInLock = FALSE;
    PLWIO_SRV_FILE_2 pFile = NULL;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &pTree->mutex);

    pFile = pTree->lruFile[ ullFid % SRV_LRU_CAPACITY ];
    if (pFile && (pFile->ullFid == ullFid))
    {
        pTree->lruFile[ ullFid % SRV_LRU_CAPACITY ] = NULL;
    }

    ntStatus = LwRtlRBTreeRemove(
                    pTree->pFileCollection,
                    &ullFid);
    BAIL_ON_NT_STATUS(ntStatus);

cleanup:

    LWIO_UNLOCK_RWMUTEX(bInLock, &pTree->mutex);

    return ntStatus;

error:

    goto cleanup;
}

BOOLEAN
SrvTree2IsNamedPipe(
    PLWIO_SRV_TREE_2 pTree
    )
{
    BOOLEAN bResult = FALSE;
    BOOLEAN bInLock = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &pTree->pShareInfo->mutex);

    bResult = (pTree->pShareInfo->service == SHARE_SERVICE_NAMED_PIPE);

    LWIO_UNLOCK_RWMUTEX(bInLock, &pTree->pShareInfo->mutex);

    return bResult;
}

PLWIO_SRV_TREE_2
SrvTree2Acquire(
    PLWIO_SRV_TREE_2 pTree
    )
{
    LWIO_LOG_DEBUG("Acquring tree [tid:%u]", pTree->ulTid);

    InterlockedIncrement(&pTree->refcount);

    return pTree;
}

VOID
SrvTree2Release(
    PLWIO_SRV_TREE_2 pTree
    )
{
    LWIO_LOG_DEBUG("Releasing tree [tid:%u]", pTree->ulTid);

    if (InterlockedDecrement(&pTree->refcount) == 0)
    {
        SrvTree2Free(pTree);
    }
}

static
NTSTATUS
SrvTree2AcquireFileId_inlock(
   PLWIO_SRV_TREE_2 pTree,
   PULONG64         pullFid
   )
{
    NTSTATUS ntStatus = 0;
    ULONG64  candidateFid = pTree->ullNextAvailableFid;
    BOOLEAN  bFound = FALSE;

    do
    {
        PLWIO_SRV_FILE_2 pFile = NULL;

        /* 0 is never a valid fid */

        if ((candidateFid == 0) || (candidateFid == UINT64_MAX))
        {
            candidateFid = 1;
        }

        ntStatus = LwRtlRBTreeFind(
                        pTree->pFileCollection,
                        &candidateFid,
                        (PVOID*)&pFile);
        if (ntStatus == STATUS_NOT_FOUND)
        {
            ntStatus = STATUS_SUCCESS;
            bFound = TRUE;
        }
        else
        {
            candidateFid++;
        }
        BAIL_ON_NT_STATUS(ntStatus);

    } while ((candidateFid != pTree->ullNextAvailableFid) && !bFound);

    if (!bFound)
    {
        ntStatus = STATUS_TOO_MANY_OPENED_FILES;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    *pullFid = candidateFid;

    /* Increment by 1 by make sure tyo deal with wraparound */

    candidateFid++;
    pTree->ullNextAvailableFid = candidateFid ? candidateFid : 1;

cleanup:

    return ntStatus;

error:

    *pullFid = 0;

    goto cleanup;
}

static
int
SrvTree2FileCompare(
    PVOID pKey1,
    PVOID pKey2
    )
{
    PULONG64 pFid1 = (PULONG64)pKey1;
    PULONG64 pFid2 = (PULONG64)pKey2;

    if (*pFid1 > *pFid2)
    {
        return 1;
    }
    else if (*pFid1 < *pFid2)
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
SrvTree2FileRelease(
    PVOID pFile
    )
{
    SrvFile2Release((PLWIO_SRV_FILE_2)pFile);
}

static
VOID
SrvTree2Free(
    PLWIO_SRV_TREE_2 pTree
    )
{
    LWIO_LOG_DEBUG("Freeing tree [object:0x%x][tid:%u]",
                    pTree,
                    pTree->ulTid);

    if (pTree->pMutex)
    {
        pthread_rwlock_destroy(&pTree->mutex);
        pTree->pMutex = NULL;
    }

    if (pTree->pFileCollection)
    {
        LwRtlRBTreeFree(pTree->pFileCollection);
    }

    if (pTree->pShareInfo)
    {
        SrvShareReleaseInfo(pTree->pShareInfo);
    }

    SrvFreeMemory(pTree);
}



/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
