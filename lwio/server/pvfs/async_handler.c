/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*-
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * Editor Settings: expandtabs and use 4 spaces for indentation */

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
 *        async_handler.c
 *
 * Abstract:
 *
 *        Likewise Posix File System Driver (PVFS)
 *
 *        Async IRP dispatch routines
 *
 * Authors: Gerald Carter <gcarter@likewise.com>
 */

#include "pvfs.h"

/* Forward declarations */


/* Code */

/***********************************************************************
 **********************************************************************/

VOID
PvfsQueueCancelIrp(
    PIRP pIrp,
    PVOID pCancelContext
    )
{
    NTSTATUS ntError = STATUS_SUCCESS;
    PPVFS_IRP_CONTEXT pIrpContext = (PPVFS_IRP_CONTEXT)pCancelContext;
    USHORT SetFlag = 0;

    PvfsReferenceIrpContext(pIrpContext);

    SetFlag = PvfsIrpContextConditionalSetFlag(
                  pIrpContext,
                  PVFS_IRP_CTX_FLAG_ACTIVE,
                  PVFS_IRP_CTX_FLAG_REQUEST_CANCEL,
                  PVFS_IRP_CTX_FLAG_CANCELLED);

    if (IsSetFlag(SetFlag, PVFS_IRP_CTX_FLAG_CANCELLED))
    {
        switch(pIrpContext->QueueType)
        {
        case PVFS_QUEUE_TYPE_IO:
            // General IrpContext processing will detect the cancellation
            break;

        case PVFS_QUEUE_TYPE_OPLOCK:
            ntError = PvfsScheduleCancelOplock(pIrpContext);
            break;

        case PVFS_QUEUE_TYPE_PENDING_OPLOCK_BREAK:
            ntError = PvfsScheduleCancelPendingOp(pIrpContext);
            break;

        case PVFS_QUEUE_TYPE_PENDING_LOCK:
            ntError = PvfsScheduleCancelLock(pIrpContext);
            break;

        case PVFS_QUEUE_TYPE_NOTIFY:
            ntError = PvfsScheduleCancelNotify(pIrpContext);
            break;

        default:
            // IrpContext must always have a valid QueueType if pended
            LWIO_ASSERT(FALSE);
            break;
        }
    }

    PvfsReleaseIrpContext(&pIrpContext);

    return;
}



/***********************************************************************
 **********************************************************************/

NTSTATUS
PvfsQueueCancelIrpIfRequested(
    PPVFS_IRP_CONTEXT pIrpContext
    )
{
    NTSTATUS ntError = STATUS_SUCCESS;
    USHORT SetFlag = 0;

    /* First check to see if we've been requested to cancel the IRP */

    SetFlag = PvfsIrpContextConditionalSetFlag(
                  pIrpContext,
                  PVFS_IRP_CTX_FLAG_REQUEST_CANCEL,
                  PVFS_IRP_CTX_FLAG_CANCELLED,
                  0);

    if (IsSetFlag(SetFlag, PVFS_IRP_CTX_FLAG_CANCELLED))
    {
        PvfsIrpContextClearFlag(pIrpContext, PVFS_IRP_CTX_FLAG_ACTIVE);

        PvfsQueueCancelIrp(pIrpContext->pIrp, pIrpContext);
        ntError = STATUS_CANCELLED;
    }

    return ntError;
}

/*****************************************************************************
 ****************************************************************************/

NTSTATUS
PvfsCreateWorkContext(
    OUT PPVFS_WORK_CONTEXT *ppWorkContext,
    IN  LONG lFlags,
    IN  PVOID pContext,
    IN  PPVFS_WORK_CONTEXT_CALLBACK pfnCompletion,
    IN  PPVFS_WORK_CONTEXT_FREE_CTX pfnFreeContext
    )
{
    NTSTATUS ntError = STATUS_UNSUCCESSFUL;
    PPVFS_WORK_CONTEXT pWorkCtx = NULL;

    ntError = PvfsAllocateMemory(
                  (PVOID*)&pWorkCtx,
                  sizeof(PVFS_WORK_CONTEXT),
                  TRUE);
    BAIL_ON_NT_STATUS(ntError);

    pWorkCtx->Flags = lFlags;
    pWorkCtx->pContext = pContext;

    if (IsSetFlag(lFlags, PVFS_WORK_CTX_FLAG_IRP_CONTEXT))
    {
        PvfsReferenceIrpContext((PPVFS_IRP_CONTEXT)pContext);
    }

    pWorkCtx->pfnCompletion = pfnCompletion;
    pWorkCtx->pfnFreeContext = pfnFreeContext;

    *ppWorkContext = pWorkCtx;

    InterlockedIncrement(&gPvfsWorkContextCount);

    ntError = STATUS_SUCCESS;

cleanup:
    return ntError;

error:
    goto cleanup;
}

////////////////////////////////////////////////////////////////////////

NTSTATUS
PvfsCreateIrpWorkContext(
    OUT PPVFS_WORK_CONTEXT *ppWorkContext,
    IN  LONG lFlags,
    IN  PVOID pContext,
    IN  PPVFS_WORK_CONTEXT_CALLBACK pfnCompletion,
    IN  PPVFS_WORK_CONTEXT_FREE_CTX pfnFreeContext
    )
{
    NTSTATUS ntError = STATUS_UNSUCCESSFUL;
    PPVFS_WORK_CONTEXT pWorkCtx = NULL;

    ntError = PvfsAllocateMemory(
                  (PVOID*)&pWorkCtx,
                  sizeof(PVFS_WORK_CONTEXT),
                  TRUE);
    BAIL_ON_NT_STATUS(ntError);

    pWorkCtx->Flags = lFlags;
    pWorkCtx->pContext = pContext;

    if (IsSetFlag(lFlags, PVFS_WORK_CTX_FLAG_IRP_CONTEXT))
    {
        PvfsReferenceIrpContext((PPVFS_IRP_CONTEXT)pContext);
    }

    pWorkCtx->pfnCompletion = pfnCompletion;
    pWorkCtx->pfnFreeContext = pfnFreeContext;

    *ppWorkContext = pWorkCtx;

    InterlockedIncrement(&gPvfsWorkContextCount);

    ntError = STATUS_SUCCESS;

cleanup:
    return ntError;

error:
    goto cleanup;
}


/*****************************************************************************
 ****************************************************************************/

VOID
PvfsFreeWorkContext(
    IN OUT PPVFS_WORK_CONTEXT *ppWorkContext
    )
{
    PPVFS_WORK_CONTEXT pWorkCtx = NULL;

    if (ppWorkContext && *ppWorkContext)
    {
        pWorkCtx = *ppWorkContext;

        if (pWorkCtx->pContext)
        {
            if (IsSetFlag(pWorkCtx->Flags, PVFS_WORK_CTX_FLAG_IRP_CONTEXT))
            {
                PvfsReleaseIrpContext((PPVFS_IRP_CONTEXT*)&pWorkCtx->pContext);
            }
            else
            {
                if (pWorkCtx->pfnFreeContext)
                {
                    pWorkCtx->pfnFreeContext(&pWorkCtx->pContext);
                }
                else
                {
                    PVFS_FREE(&pWorkCtx->pContext);
                }
            }
        }

        PVFS_FREE(ppWorkContext);

        InterlockedDecrement(&gPvfsWorkContextCount);
    }

    return;
}

