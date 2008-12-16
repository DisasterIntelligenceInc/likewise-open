/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

/*
 * Copyright Likewise Software    2004-2008
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
 *        ipc_log.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Inter-process communication (Server) API for Log Info
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 */
#include "ipc.h"

LWMsgStatus
LsaSrvIpcSetLogInfo(
    LWMsgAssoc* assoc,
    const LWMsgMessage* pRequest,
    LWMsgMessage* pResponse,
    void* data
    )
{
    DWORD dwError = 0;
    PLSA_IPC_SET_LOGINFO_REQ pReq = pRequest->object;
    PLSA_IPC_ERROR pError = NULL;

    dwError = LsaSrvSetLogInfo(
                        (HANDLE)pReq->Handle,
                        pReq->pLogInfo);

    if (!dwError)
    {
        pResponse->tag = LSA_R_SET_LOGINFO_SUCCESS;
        pResponse->object = NULL;
    }
    else
    {
        dwError = LsaSrvIpcCreateError(dwError, NULL, &pError);
        BAIL_ON_LSA_ERROR(dwError);

        pResponse->tag = LSA_R_SET_LOGINFO_FAILURE;
        pResponse->object = pError;
    }

cleanup:
    return MAP_LSA_ERROR_IPC(dwError);

error:
    goto cleanup;
}

LWMsgStatus
LsaSrvIpcGetLogInfo(
    LWMsgAssoc* assoc,
    const LWMsgMessage* pRequest,
    LWMsgMessage* pResponse,
    void* data
    )
{
    DWORD dwError = 0;
    PLSA_LOG_INFO pLogInfo = NULL;
    PLSA_IPC_ERROR pError = NULL;


    dwError = LsaSrvGetLogInfo(
                    (HANDLE)pRequest->object,
                    &pLogInfo);

    if (!dwError)
    {
        pResponse->tag = LSA_R_GET_LOGINFO_SUCCESS;
        pResponse->object = pLogInfo;
        pLogInfo = NULL;
    }
    else
    {
        dwError = LsaSrvIpcCreateError(dwError, NULL, &pError);
        BAIL_ON_LSA_ERROR(dwError);

        pResponse->tag = LSA_R_GET_LOGINFO_FAILURE;
        pResponse->object = pError;
    }

cleanup:
    if (pLogInfo)
    {
        LsaFreeLogInfo(pLogInfo);
    }
    return MAP_LSA_ERROR_IPC(dwError);

error:
    goto cleanup;
}
