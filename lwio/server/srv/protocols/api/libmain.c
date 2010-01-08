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
 *        Protocols API
 *
 *        Library Main
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 *
 */

#include "includes.h"

static
NTSTATUS
SrvProtocolExecute_SMB_V1_Filter(
    PSRV_EXEC_CONTEXT pContext
    );

static
VOID
SrvProtocolFreeExecContext(
    PSRV_PROTOCOL_EXEC_CONTEXT pContext
    );

NTSTATUS
SrvProtocolInit(
    PSMB_PROD_CONS_QUEUE pWorkQueue,
    BOOLEAN              bSupportSMB2
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN  bSupportSMBV2 = FALSE;

    gProtocolApiGlobals.pWorkQueue   = pWorkQueue;
    gProtocolApiGlobals.bSupportSMB2 = bSupportSMB2;

    status = SrvProtocolConfigSupports_SMB_V2(&bSupportSMBV2);
    BAIL_ON_NT_STATUS(status);

    status = SrvProtocolInit_SMB_V1(pWorkQueue);
    BAIL_ON_NT_STATUS(status);

    if (bSupportSMBV2)
    {
        status = SrvProtocolInit_SMB_V2(pWorkQueue);
        BAIL_ON_NT_STATUS(status);
    }

error:

    return status;
}

NTSTATUS
SrvProtocolExecute(
    PSRV_EXEC_CONTEXT pContext
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (!pContext->pProtocolContext)
    {
        ntStatus = SrvAllocateMemory(
                        sizeof(SRV_PROTOCOL_EXEC_CONTEXT),
                        (PVOID*)&pContext->pProtocolContext);
        BAIL_ON_NT_STATUS(ntStatus);

        pContext->pProtocolContext->protocolVersion =
                            pContext->pConnection->protocolVer;

        pContext->pfnFreeContext = &SrvProtocolFreeExecContext;
    }

    if ((pContext->pSmbRequest->pSMBHeader->command == COM_NEGOTIATE) &&
        (SrvConnectionGetState(pContext->pConnection) !=
                                        LWIO_SRV_CONN_STATE_INITIAL))
    {
        SrvConnectionSetInvalid(pContext->pConnection);

        ntStatus = STATUS_CONNECTION_RESET;
        BAIL_ON_NT_STATUS(ntStatus);
    }

    switch (pContext->pSmbRequest->protocolVer)
    {
        case SMB_PROTOCOL_VERSION_1:

            ntStatus = SrvProtocolExecute_SMB_V1_Filter(pContext);

            break;

        case SMB_PROTOCOL_VERSION_2:

            ntStatus = SrvProtocolExecute_SMB_V2(pContext);

            break;
    }
    BAIL_ON_NT_STATUS(ntStatus);

    // Cleanup any protocol state before sending a response.
    if (pContext->pProtocolContext)
    {
        pContext->pfnFreeContext(pContext->pProtocolContext);
        pContext->pProtocolContext = NULL;
    }

    if (pContext->pSmbResponse && pContext->pSmbResponse->pNetBIOSHeader->len)
    {
        ULONG iRepeat = 0;

        //
        // Note: An echo request might result in duplicates being sent
        // TODO: Find out if the repeats must have different sequence numbers
        for (; iRepeat < (pContext->ulNumDuplicates + 1); iRepeat++)
        {
            /* synchronous response */
            ntStatus = SrvTransportSendResponse(
                            pContext->pConnection,
                            pContext->pSmbResponse);
            BAIL_ON_NT_STATUS(ntStatus);
        }
    }

cleanup:

    return ntStatus;

error:

    switch (ntStatus)
    {
        case STATUS_PENDING:

            // Asynchronous processing

            if (pContext->pSmbAuxResponse)
            {
                NTSTATUS ntStatus2 = STATUS_SUCCESS;

                /* synchronous response */
                ntStatus2 = SrvTransportSendResponse(
                                pContext->pConnection,
                                pContext->pSmbAuxResponse);
                if (ntStatus2)
                {
                    LWIO_LOG_ERROR("Failed to send auxiliary response "
                                   "[code:0x%08x",
                                   ntStatus2);
                }

                SMBPacketRelease(
                        pContext->pConnection->hPacketAllocator,
                        pContext->pSmbAuxResponse);

                pContext->pSmbAuxResponse = NULL;
            }

            ntStatus = STATUS_SUCCESS;

            break;

        default:

            break;
    }

    goto cleanup;
}

static
NTSTATUS
SrvProtocolExecute_SMB_V1_Filter(
    PSRV_EXEC_CONTEXT pContext
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PLWIO_SRV_CONNECTION pConnection = pContext->pConnection;
    PSMB_PACKET pSmbRequest = pContext->pSmbRequest;

    switch (pSmbRequest->pSMBHeader->command)
    {
        case COM_NEGOTIATE:

                ntStatus = SrvProcessNegotiate(
                                pConnection,
                                pSmbRequest,
                                &pContext->pSmbResponse);

                if (ntStatus)
                {
                    ntStatus = SrvProtocolBuildErrorResponse_SMB_V1(
                                    pConnection,
                                    pSmbRequest->pSMBHeader,
                                    ntStatus,
                                    &pContext->pSmbResponse);
                }

                break;

        default:

                ntStatus = SrvProtocolExecute_SMB_V1(pContext);

                break;
    }
    BAIL_ON_NT_STATUS(ntStatus);

error:

    return ntStatus;
}

static
VOID
SrvProtocolFreeExecContext(
    PSRV_PROTOCOL_EXEC_CONTEXT pProtocolContext
    )
{
    switch (pProtocolContext->protocolVersion)
    {
        case SMB_PROTOCOL_VERSION_1:

            if (pProtocolContext->pSmb1Context)
            {
                SrvProtocolFreeContext_SMB_V1(pProtocolContext->pSmb1Context);
            }

            break;

        case SMB_PROTOCOL_VERSION_2:

            if (pProtocolContext->pSmb2Context)
            {
                SrvProtocolFreeContext_SMB_V2(pProtocolContext->pSmb2Context);
            }

            break;
    }

    SrvFreeMemory(pProtocolContext);
}

NTSTATUS
SrvProtocolShutdown(
    VOID
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN bSupportSMBV2 = FALSE;

    status = SrvProtocolConfigSupports_SMB_V2(&bSupportSMBV2);
    BAIL_ON_NT_STATUS(status);

    status = SrvProtocolShutdown_SMB_V1();
    BAIL_ON_NT_STATUS(status);

    if (bSupportSMBV2)
    {
        status = SrvProtocolShutdown_SMB_V2();
        BAIL_ON_NT_STATUS(status);
    }

    gProtocolApiGlobals.pWorkQueue = NULL;

error:

    return status;
}

