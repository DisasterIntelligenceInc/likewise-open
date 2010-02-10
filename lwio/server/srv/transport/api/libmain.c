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
SrvTransportInit(
    PLWIO_PACKET_ALLOCATOR     hPacketAllocator,
    PLWIO_SRV_SHARE_ENTRY_LIST pShareList,
    PSMB_PROD_CONS_QUEUE       pWorkQueue
    )
{
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    status = SrvThreadpoolTransportInit(
                    hPacketAllocator,
                    pShareList,
                    pWorkQueue,
                    &gpSrvTransportApi);

    return status;
}

NTSTATUS
SrvTransportGetRequest(
    IN  struct timespec*   pTimeSpec,
    OUT PSRV_EXEC_CONTEXT* ppContext
    )
{
    return gpSrvTransportApi->pfnTransportGetRequest(pTimeSpec, ppContext);
}

NTSTATUS
SrvTransportSendResponse(
    IN PLWIO_SRV_CONNECTION pConnection,
    IN PSMB_PACKET          pResponse
    )
{
    return gpSrvTransportApi->pfnTransportSendResponse(pConnection, pResponse);
}

NTSTATUS
SrvTransportShutdown(
    VOID
    )
{
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    status = SrvThreadpoolTransportShutdown(gpSrvTransportApi);

    gpSrvTransportApi = NULL;

    return status;
}

