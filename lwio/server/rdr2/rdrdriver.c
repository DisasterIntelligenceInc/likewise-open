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
 *        driver.c
 *
 * Abstract:
 *
 *        Likewise Posix File System Driver (PVFS)
 *
 *        Driver Entry Function
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Danilo Almeida (dalmeida@likewisesoftware.com)
 */

#include "rdr.h"

static IO_DEVICE_HANDLE gDeviceHandle = NULL;

static
NTSTATUS
RdrInitialize(
    VOID
    );

static
NTSTATUS
RdrShutdown(
    VOID
    );

static
VOID
RdrDriverShutdown(
    IN IO_DRIVER_HANDLE DriverHandle
    )
{
    RdrShutdown();

    if (gDeviceHandle)
    {
        IoDeviceDelete(&gDeviceHandle);
    }
}

static
NTSTATUS
RdrDriverDispatch(
    IN IO_DEVICE_HANDLE DeviceHandle,
    IN PIRP pIrp
    )
{
    NTSTATUS ntStatus = 0;

    switch (pIrp->Type)
    {
    case IRP_TYPE_CREATE:
        ntStatus = RdrCreate(
            DeviceHandle,
            pIrp
            );
        break;

    case IRP_TYPE_CLOSE:
        ntStatus = RdrClose(
            DeviceHandle,
            pIrp
            );
        break;


    case IRP_TYPE_READ:
        ntStatus = RdrRead(
            DeviceHandle,
            pIrp
            );
        break;

    case IRP_TYPE_WRITE:
        ntStatus = RdrWrite(
            DeviceHandle,
            pIrp
            );
        break;

    case IRP_TYPE_DEVICE_IO_CONTROL:
        ntStatus = STATUS_NOT_IMPLEMENTED;
        break;

    case IRP_TYPE_FS_CONTROL:
        ntStatus = RdrFsctl(
            DeviceHandle,
            pIrp
            );
        break;

    case IRP_TYPE_FLUSH_BUFFERS:
        ntStatus = STATUS_NOT_IMPLEMENTED;
        break;

    case IRP_TYPE_QUERY_INFORMATION:
        ntStatus = RdrQueryInformation(
            DeviceHandle,
            pIrp
            );
        break;
    case IRP_TYPE_QUERY_DIRECTORY:
        ntStatus = RdrQueryDirectory(
            DeviceHandle,
            pIrp
            );
        break;
    case IRP_TYPE_QUERY_VOLUME_INFORMATION:
        ntStatus = RdrQueryVolumeInformation(
            DeviceHandle,
            pIrp);
        break;
    case IRP_TYPE_SET_INFORMATION:
        ntStatus = RdrSetInformation(
            DeviceHandle,
            pIrp
            );
        break;
    case IRP_TYPE_QUERY_SECURITY:
        ntStatus = RdrQuerySecurity(
            DeviceHandle,
            pIrp
            );
        break;
    default:
        ntStatus = STATUS_UNSUCCESSFUL;
    }

    return ntStatus;
}

extern NTSTATUS IO_DRIVER_ENTRY(rdr)(
    IN IO_DRIVER_HANDLE DriverHandle,
    IN ULONG InterfaceVersion
    );

NTSTATUS
IO_DRIVER_ENTRY(rdr)(
    IN IO_DRIVER_HANDLE DriverHandle,
    IN ULONG InterfaceVersion
    )
{
    NTSTATUS ntStatus = 0;

    if (IO_DRIVER_ENTRY_INTERFACE_VERSION != InterfaceVersion)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    ntStatus = IoDriverInitialize(DriverHandle,
                                  NULL,
                                  RdrDriverShutdown,
                                  RdrDriverDispatch);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = IoDeviceCreate(&gDeviceHandle,
                              DriverHandle,
                              "rdr",
                              NULL);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = RdrInitialize();
    BAIL_ON_NT_STATUS(ntStatus);

error:

    return ntStatus;
}

static
NTSTATUS
RdrInitialize(
    VOID
    )
{
    NTSTATUS ntStatus = 0;
    PLW_THREAD_POOL_ATTRIBUTES pAttrs = NULL;

    memset(&gRdrRuntime, 0, sizeof(gRdrRuntime));

    pthread_mutex_init(&gRdrRuntime.socketHashLock, NULL);

    /* Pid used for SMB Header */

    gRdrRuntime.SysPid = getpid();

    ntStatus = SMBPacketCreateAllocator(1, &gRdrRuntime.hPacketAllocator);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = RdrSocketInit();
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlCreateThreadPoolAttributes(&pAttrs);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlCreateThreadPool(&gRdrRuntime.pThreadPool, pAttrs);
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = LwRtlCreateTaskGroup(gRdrRuntime.pThreadPool, &gRdrRuntime.pReaderTaskGroup);
    BAIL_ON_NT_STATUS(ntStatus);

error:

    LwRtlFreeThreadPoolAttributes(&pAttrs);

    return ntStatus;
}

static
NTSTATUS
RdrShutdown(
    VOID
    )
{
    NTSTATUS ntStatus = 0;

    ntStatus = RdrSocketShutdown();
    BAIL_ON_NT_STATUS(ntStatus);

    pthread_mutex_destroy(&gRdrRuntime.socketHashLock);

    if (gRdrRuntime.hPacketAllocator != (HANDLE)NULL)
    {
        SMBPacketFreeAllocator(gRdrRuntime.hPacketAllocator);
        gRdrRuntime.hPacketAllocator = NULL;
    }

error:

    return ntStatus;
}

NTSTATUS
RdrCreateContext(
    PIRP pIrp,
    PRDR_OP_CONTEXT* ppContext
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PRDR_OP_CONTEXT pContext = NULL;

    status = LW_RTL_ALLOCATE_AUTO(&pContext);
    BAIL_ON_NT_STATUS(status);

    LwListInit(&pContext->Link);

    pContext->pIrp = pIrp;

    *ppContext = pContext;

error:

    return status;
}

VOID
RdrFreeContext(
    PRDR_OP_CONTEXT pContext
    )
{
    if (pContext)
    {
        if (pContext->Packet.bufferLen)
        {
            SMBPacketBufferFree(gRdrRuntime.hPacketAllocator,
                                pContext->Packet.pRawBuffer,
                                pContext->Packet.bufferLen);
        }

        RTL_FREE(&pContext);
    }
}

PRDR_OP_CONTEXT
RdrContinueContext(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    )
{
    if (pContext->Continue)
    {
        return pContext->Continue(pContext, status, pParam);
    }
    else
    {
        return NULL;
    }
}

VOID
RdrContinueContextList(
    PLW_LIST_LINKS pList,
    NTSTATUS status,
    PVOID pParam
    )
{
    PLW_LIST_LINKS pElement = NULL;
    PRDR_OP_CONTEXT pContext = NULL;

    while (!LwListIsEmpty(pList))
    {
        pElement = LwListRemoveHead(pList);
        pContext = LW_STRUCT_FROM_FIELD(pElement, RDR_OP_CONTEXT, Link);

        RdrContinueContext(pContext, status, pParam);
    }
}
/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
