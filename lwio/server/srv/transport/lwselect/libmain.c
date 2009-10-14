/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

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
 *        libmain.c
 *
 * Abstract:
 *
 *        Likewise IO (LWIO) - SRV
 *
 *        Transport API
 *
 *        Library Main
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 *
 */

#include "includes.h"

NTSTATUS
SrvSelectTransportInit(
    PLWIO_PACKET_ALLOCATOR         hPacketAllocator,
    PLWIO_SRV_SHARE_ENTRY_LIST     pShareList,
    PSMB_PROD_CONS_QUEUE           pWorkQueue,
    PSRV_TRANSPORT_FUNCTION_TABLE* ppFnTable
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    INT iReader = 0;

    status = SrvSelectTransportInitConfig(&gSrvSelectTransport.config);
    BAIL_ON_NT_STATUS(status);

    status = SrvSelectTransportReadConfig(&gSrvSelectTransport.config);
    BAIL_ON_NT_STATUS(status);

    gSrvSelectTransport.hPacketAllocator = hPacketAllocator;
    gSrvSelectTransport.pShareList   = pShareList;
    gSrvSelectTransport.ulNumReaders = gSrvSelectTransport.config.ulNumReaders;

    gSrvSelectTransport.pWorkQueue = pWorkQueue;

    status = SrvAllocateMemory(
                    gSrvSelectTransport.ulNumReaders * sizeof(LWIO_SRV_SOCKET_READER),
                    (PVOID*)&gSrvSelectTransport.pReaderArray);
    BAIL_ON_NT_STATUS(status);

    for (; iReader < gSrvSelectTransport.ulNumReaders; iReader++)
    {
        PLWIO_SRV_SOCKET_READER pReader = NULL;

        pReader = &gSrvSelectTransport.pReaderArray[iReader];

        pReader->readerId = iReader + 1;

        status = SrvSocketReaderInit(gSrvSelectTransport.pWorkQueue, pReader);
        BAIL_ON_NT_STATUS(status);
    }

    status = SrvListenerInit(
                    gSrvSelectTransport.hPacketAllocator,
                    gSrvSelectTransport.pShareList,
                    gSrvSelectTransport.pReaderArray,
                    gSrvSelectTransport.ulNumReaders,
                    &gSrvSelectTransport.listener,
                    gSrvSelectTransport.config.bEnableSigning,
                    gSrvSelectTransport.config.bRequireSigning);
    BAIL_ON_NT_STATUS(status);

    *ppFnTable = &gSrvSelectTransport.fnTable;

cleanup:

    return status;

error:

    *ppFnTable = NULL;

    goto cleanup;
}

NTSTATUS
SrvSelectTransportGetRequest(
    IN  struct timespec*   pTimespec,
    OUT PSRV_EXEC_CONTEXT* ppContext
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
SrvSelectTransportSendResponse(
    IN          PLWIO_SRV_CONNECTION pConnection,
    IN          PSMB_PACKET          pResponse
    )
{
    return SrvConnectionWriteMessage(pConnection, pResponse);
}

NTSTATUS
SrvSelectTransportShutdown(
    PSRV_TRANSPORT_FUNCTION_TABLE pFnTable
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    status = SrvListenerShutdown(&gSrvSelectTransport.listener);
    BAIL_ON_NT_STATUS(status);

    if (gSrvSelectTransport.pReaderArray)
    {
        if (gSrvSelectTransport.ulNumReaders)
        {
            INT      iReader = 0;

            for (; iReader < gSrvSelectTransport.ulNumReaders; iReader++)
            {
                status = SrvSocketReaderFreeContents(
                                &gSrvSelectTransport.pReaderArray[iReader]);
                BAIL_ON_NT_STATUS(status);
            }
        }

        SrvFreeMemory(gSrvSelectTransport.pReaderArray);
    }

    gSrvSelectTransport.pWorkQueue = NULL;

    SrvSelectTransportFreeConfigContents(&gSrvSelectTransport.config);

error:

    return status;
}

