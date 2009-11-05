/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software    2004-2008
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        net_usergetinfo.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Client Interface
 *
 *        NetUserGetInfo function
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


NET_API_STATUS
NetUserGetInfo(
    PCWSTR  pwszHostname,
    PCWSTR  pwszUsername,
    DWORD   dwLevel,
    PVOID  *ppBuffer
    )
{
    const DWORD dwUserAccessFlags = USER_ACCESS_GET_NAME_ETC |
                                    USER_ACCESS_GET_LOCALE |
                                    USER_ACCESS_GET_LOGONINFO |
                                    USER_ACCESS_GET_ATTRIBUTES |
                                    USER_ACCESS_GET_GROUPS |
                                    USER_ACCESS_GET_GROUP_MEMBERSHIP;
    const DWORD dwSamrInfoLevel = 21;

    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    NetConn *pConn = NULL;
    handle_t hSamrBinding = NULL;
    ACCOUNT_HANDLE hUser = NULL;
    DWORD dwUserRid = 0;
    UserInfo *pSamrUserInfo = NULL;
    DWORD dwSize = 0;
    DWORD dwSpaceAvailable = 0;
    PVOID pBuffer = NULL;
    PIO_CREDS pCreds = NULL;

    BAIL_ON_INVALID_PTR(pwszUsername);
    BAIL_ON_INVALID_PTR(ppBuffer);

    if (dwLevel != 20)
    {
        err = ERROR_INVALID_LEVEL;
        BAIL_ON_WINERR_ERROR(err);
    }

    status = LwIoGetActiveCreds(NULL, &pCreds);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = NetConnectSamr(&pConn,
                            pwszHostname,
                            0,
                            0,
                            pCreds);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = NetOpenUser(pConn,
                         pwszUsername,
                         dwUserAccessFlags,
                         &hUser,
                         &dwUserRid);
    BAIL_ON_NTSTATUS_ERROR(status);

    hSamrBinding = pConn->samr.bind;

    status = SamrQueryUserInfo(hSamrBinding,
                               hUser,
                               dwSamrInfoLevel,
                               &pSamrUserInfo);
    BAIL_ON_NTSTATUS_ERROR(status);

    err = NetAllocateUserInfo(NULL,
                              NULL,
                              dwLevel,
                              pSamrUserInfo,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    dwSpaceAvailable = dwSize;
    dwSize           = 0;

    status = NetAllocateMemory((void**)&pBuffer,
                               dwSpaceAvailable,
                               NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    err = NetAllocateUserInfo(pBuffer,
                              &dwSpaceAvailable,
                              dwLevel,
                              pSamrUserInfo,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    status = SamrClose(hSamrBinding, hUser);
    BAIL_ON_NTSTATUS_ERROR(status);

    *ppBuffer = pBuffer;

cleanup:
    if (pSamrUserInfo)
    {
        SamrFreeMemory(pSamrUserInfo);
    }

    if (pCreds)
    {
        LwIoDeleteCreds(pCreds);
    }

    if (err == ERROR_SUCCESS &&
        status != STATUS_SUCCESS)
    {
        err = NtStatusToWin32Error(status);
    }

    return err;

error:
    if (pBuffer)
    {
        NetFreeMemory((void*)pBuffer);
    }

    *ppBuffer = NULL;
    goto cleanup;
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
