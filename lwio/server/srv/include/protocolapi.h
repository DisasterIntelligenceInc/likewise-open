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
 *        srv/protocol.h
 *
 * Abstract:
 *
 *        Likewise IO (LWIO) - SRV
 *
 *        Protocols
 *
 *        Definitions
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 *
 */

#ifndef __PROTOCOL_API_H__
#define __PROTOCOL_API_H__

typedef VOID (*PFN_SRV_PROTOCOL_SEND_COMPLETE)(
    IN PVOID pContext,
    IN NTSTATUS Status
    );

NTSTATUS
SrvProtocolInit(
    PSMB_PROD_CONS_QUEUE pWorkQueue,
    PLWIO_PACKET_ALLOCATOR hPacketAllocator,
    PLWIO_SRV_SHARE_ENTRY_LIST pShareList
    );

NTSTATUS
SrvProtocolExecute(
    PSRV_EXEC_CONTEXT pContext
    );

NTSTATUS
SrvProtocolTransportSendResponse(
    IN PLWIO_SRV_CONNECTION pConnection,
    IN PSMB_PACKET pPacket
    );

NTSTATUS
SrvProtocolTransportSendZctResponse(
    IN PLWIO_SRV_CONNECTION pConnection,
    IN PLW_ZCT_VECTOR pZct,
    IN OPTIONAL PFN_SRV_PROTOCOL_SEND_COMPLETE pfnCallback,
    IN OPTIONAL PVOID pCallbackContext
    );

NTSTATUS
SrvProtocolEnumerateSessions(
    PWSTR  pwszUncClientname,
    PWSTR  pwszUsername,
    ULONG  ulInfoLevel,
    PBYTE  pBuffer,
    ULONG  ulBufferSize,
    PULONG pulBytesUsed,
    PULONG pulEntriesRead,
    PULONG pulTotalEntries,
    PULONG pulResumeHandle
    );

NTSTATUS
SrvProtocolEnumerateFiles(
    PWSTR  pwszBasepath,
    PWSTR  pwszUsername,
    ULONG  ulInfoLevel,
    PBYTE  pBuffer,
    ULONG  ulBufferSize,
    PULONG pulBytesUsed,
    PULONG pulEntriesRead,
    PULONG pulTotalEntries,
    PULONG pulResumeHandle
    );

VOID
SrvProtocolShutdown(
    VOID
    );

#endif /* __PROTOCOL_API_H__ */
