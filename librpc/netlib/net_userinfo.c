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
 *        net_userinfo.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Client Interface
 *
 *        NetAPI user info buffer handling functions
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"

static
DWORD
NetAllocateUserInfo0(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo1(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo2(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo3(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo4(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo10(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo11(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo20(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateUserInfo23(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo7FromUserInfo0(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo1(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo2(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo3(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo4(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo26FromUserInfo1(
    PVOID   *ppCursor,
    PDWORD   pdwSpaceLeft,
    PVOID    pSource,
    NetConn *pConn,
    PDWORD   pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo26FromUserInfo1003(
    PVOID   *ppCursor,
    PDWORD   pdwSpaceLeft,
    PVOID    pSource,
    NetConn *pConn,
    PDWORD   pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo26FromPassword(
    PVOID   *ppCursor,
    PDWORD   pdwSpaceLeft,
    PWSTR    pwszPassword,
    NetConn *pConn,
    PDWORD   pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo13FromUserInfo1007(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo16FromUserInfo1008(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


static
DWORD
NetAllocateSamrUserInfo8FromUserInfo1011(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    );


DWORD
NetAllocateUserInfo(
    PVOID   pInfoBuffer,
    PDWORD  pdwSpaceLeft,
    DWORD   dwLevel,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = pInfoBuffer;

    switch (dwLevel)
    {
    case 0:
        err = NetAllocateUserInfo0(&pCursor,
                                   pdwSpaceLeft,
                                   pSource,
                                   pdwSize);
        break;

    case 1:
        err = NetAllocateUserInfo1(&pCursor,
                                   pdwSpaceLeft,
                                   pSource,
                                   pdwSize);
        break;

    case 2:
        err = NetAllocateUserInfo2(&pCursor,
                                   pdwSpaceLeft,
                                   pSource,
                                   pdwSize);
        break;

    case 3:
        err = NetAllocateUserInfo3(&pCursor,
                                   pdwSpaceLeft,
                                   pSource,
                                   pdwSize);
        break;

    case 4:
        err = NetAllocateUserInfo4(&pCursor,
                                   pdwSpaceLeft,
                                   pSource,
                                   pdwSize);
        break;

    case 10:
        err = NetAllocateUserInfo10(&pCursor,
                                    pdwSpaceLeft,
                                    pSource,
                                    pdwSize);
        break;

    case 11:
        err = NetAllocateUserInfo11(&pCursor,
                                    pdwSpaceLeft,
                                    pSource,
                                    pdwSize);
        break;

    case 20:
        err = NetAllocateUserInfo20(&pCursor,
                                    pdwSpaceLeft,
                                    pSource,
                                    pdwSize);
        break;

    case 23:
        err = NetAllocateUserInfo23(&pCursor,
                                    pdwSpaceLeft,
                                    pSource,
                                    pdwSize);
        break;

    default:
        err = ERROR_INVALID_LEVEL;
        break;
    }
    BAIL_ON_WINERR_ERROR(err);

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo0(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PWSTR pwszName = (PWSTR)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* usri0_name */
    err = NetAllocBufferWC16String(&pCursor,
                                   &dwSpaceLeft,
                                   pwszName,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo1(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;
    NtTime CurrentTime = 0;
    NtTime PasswordAge = 0;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* usri1_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->account_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri1_password: SKIP */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri1_password_age */
    err = LwGetNtTime((PULONG64)&CurrentTime);
    BAIL_ON_WINERR_ERROR(err);

    PasswordAge = CurrentTime - pSamrInfo21->last_password_change;
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri1_priv: SKIP (it is set outside this function) */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri1_home_dir */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->home_directory,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri1_comment */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri1_flags */
    err = NetAllocBufferUserFlagsFromAcbFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pSamrInfo21->account_flags,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_1, usri1_flags,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri1_script_path */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->logon_script,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo2(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    err = NetAllocateUserInfo1(&pCursor,
                               &dwSpaceLeft,
                               pSource,
                               &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_auth_flags: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_2, usri2_auth_flags,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri2_full_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_usr_comment: SKIP */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_parms */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->parameters,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_workstations */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->workstations,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_last_logon */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_last_logoff: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_account_expires */
    err = NetAllocBufferWinTimeFromNtTime(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pSamrInfo21->account_expiry,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_max_storage: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);

    /* usri2_units_per_week: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_2, usri2_units_per_week,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri2_logon_hours: SKIP */
    err = NetAllocBufferLogonHours(&pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_bad_pw_count */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->bad_password_count,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_num_logons */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->logon_count,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_2, usri2_num_logons,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri2_logon_server */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_country_code */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->country_code,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri2_code_page */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->code_page,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo3(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    err = NetAllocateUserInfo2(&pCursor,
                               &dwSpaceLeft,
                               pSource,
                               &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri3_user_id */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->rid,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_3, usri3_user_id,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri3_profile */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->profile_path,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri3_home_dir_drive */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->home_drive,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri3_password_expired */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->password_expired,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo4(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    err = NetAllocateUserInfo2(&pCursor,
                               &dwSpaceLeft,
                               pSource,
                               &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_4, usri4_code_page,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri3_user_sid - it's copied outside this function,
       so reserve max space */
    err = NetAllocBufferSid(&pCursor,
                            &dwSpaceLeft,
                            NULL,
                            0,
                            &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri3_profile */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->profile_path,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri3_home_dir_drive */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->home_drive,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri3_password_expired */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->password_expired,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo10(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* usri10_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->account_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri10_comment */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri10_usr_comment: SKIP */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri10_full_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo11(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;
    NtTime CurrentTime = 0;
    NtTime PasswordAge = 0;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    err = NetAllocateUserInfo10(&pCursor,
                                &dwSpaceLeft,
                                pSource,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_priv: SKIP (it is set outside this function) */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_auth_flags: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_password_age */
    err = LwGetNtTime((PULONG64)&CurrentTime);
    BAIL_ON_WINERR_ERROR(err);

    PasswordAge = CurrentTime - pSamrInfo21->last_password_change;
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_11, usri11_password_age,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri11_home_dir */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->home_directory,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_parms */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->parameters,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_last_logon */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_last_logoff */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_bad_pw_count */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->bad_password_count,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_num_logons */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->logon_count,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_11, usri11_num_logons,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri11_logon_server */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_country_code */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->country_code,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_11, usri11_country_code,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri11_workstations */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->workstations,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_max_storage: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);

    /* usri11_units_per_week: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(USER_INFO_11, usri11_units_per_week,
                        pCursor, dwSize, dwSpaceLeft);

    /* usri11_logon_hours: SKIP */
    err = NetAllocBufferLogonHours(&pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri11_code_page */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->code_page,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateUserInfo20(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* usri20_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->account_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri20_full_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri20_comment */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri20_flags */
    err = NetAllocBufferUserFlagsFromAcbFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pSamrInfo21->account_flags,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri20_user_id */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              pSamrInfo21->rid,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;

}


static
DWORD
NetAllocateUserInfo23(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    UserInfo21 *pSamrInfo21 = (UserInfo21*)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* usri23_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->account_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri23_full_name */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri23_comment */
    err = NetAllocBufferWC16StringFromUnicodeString(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   &pSamrInfo21->comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* usri23_user_sid - it's copied outside this function,
       so reserve max space */
    err = NetAllocBufferSid(&pCursor,
                            &dwSpaceLeft,
                            NULL,
                            0,
                            &dwSize);
    BAIL_ON_WINERR_ERROR(err);


    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

    if (ppCursor)
    {
        *ppCursor = pCursor;
    }

cleanup:
    return err;

error:
    goto cleanup;

}


NTSTATUS
NetEncPasswordEx(
    BYTE     PasswordBuffer[532],
    PWSTR    pwszPassword,
    DWORD    dwPasswordLen,
    NetConn *pConn
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    MD5_CTX ctx;
    RC4_KEY rc4_key;
    BYTE InitValue[16];
    BYTE DigestedSessKey[16];

    BAIL_ON_INVALID_PTR(PasswordBuffer);
    BAIL_ON_INVALID_PTR(pwszPassword);
    BAIL_ON_INVALID_PTR(pConn);

    memset(&ctx, 0, sizeof(ctx));
    memset(InitValue, 0, sizeof(InitValue));
    memset(DigestedSessKey, 0, sizeof(DigestedSessKey));

    EncodePassBufferW16(PasswordBuffer, pwszPassword);

    get_random_buffer((unsigned char*)InitValue, sizeof(InitValue));

    MD5_Init(&ctx);
    MD5_Update(&ctx, InitValue, 16);
    MD5_Update(&ctx, pConn->sess_key, pConn->sess_key_len);
    MD5_Final(DigestedSessKey, &ctx);

    RC4_set_key(&rc4_key, 16, (unsigned char*)DigestedSessKey);
    RC4(&rc4_key, 516, (PBYTE)PasswordBuffer, (PBYTE)PasswordBuffer);
    memcpy((PVOID)&PasswordBuffer[516], InitValue, 16);

cleanup:
    return status;

error:
    goto cleanup;
}


DWORD
NetAllocateSamrUserInfo(
    PVOID    pInfoBuffer,
    PDWORD   pdwSamrLevel,
    PDWORD   pdwSpaceLeft,
    DWORD    dwLevel,
    PVOID    pSource,
    NetConn *pConn,
    PDWORD   pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    NTSTATUS status = STATUS_SUCCESS;
    PVOID pCursor = pInfoBuffer;
    DWORD dwSamrLevel = 0;

    BAIL_ON_INVALID_PTR(pSource);

    if (pdwSamrLevel)
    {
        dwSamrLevel = *pdwSamrLevel;
    }

    /*
     * Check if caller has requested a particular infolevel first
     */
    if (dwSamrLevel == 26)
    {
        switch (dwLevel)
        {
        case 1:
        case 2:
        case 3:
        case 4:
            /* USER_INFO_[1-4] types start the same way
               (up to password field */
            err = NetAllocateSamrUserInfo26FromUserInfo1(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pConn,
                                         pdwSize);
            break;

        case 1003:
            err = NetAllocateSamrUserInfo26FromUserInfo1003(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pConn,
                                         pdwSize);
        default:
            err = ERROR_INVALID_LEVEL;
            break;
        }
        BAIL_ON_WINERR_ERROR(err);
    }
    else
    {
        /*
         * Prepare default samr infolevel depending on netapi infolevel
         */
        switch (dwLevel)
        {
        case 0:
            err = NetAllocateSamrUserInfo7FromUserInfo0(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 7;
            break;

        case 1:
            err = NetAllocateSamrUserInfo21FromUserInfo1(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 21;
            break;

        case 2:
            err = NetAllocateSamrUserInfo21FromUserInfo2(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 21;
            break;

        case 3:
            err = NetAllocateSamrUserInfo21FromUserInfo3(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 21;
            break;

        case 4:
            err = NetAllocateSamrUserInfo21FromUserInfo4(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 21;
            break;

        case 1007:
            err = NetAllocateSamrUserInfo13FromUserInfo1007(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 13;
            break;

        case 1008:
            err = NetAllocateSamrUserInfo16FromUserInfo1008(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 16;
            break;

        case 1011:
            err = NetAllocateSamrUserInfo8FromUserInfo1011(
                                         &pCursor,
                                         pdwSpaceLeft,
                                         pSource,
                                         pdwSize);
            dwSamrLevel = 8;
            break;

        default:
            err = ERROR_INVALID_LEVEL;
            break;
        }
        BAIL_ON_WINERR_ERROR(err);
    }

    if (pdwSamrLevel)
    {
        *pdwSamrLevel = dwSamrLevel;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo7FromUserInfo0(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_0 pUserInfo0 = (PUSER_INFO_0)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* account_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo0->usri0_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


#define SAMR_FIELD_PRESENT(field, flag)                \
    if (field)                                         \
    {                                                  \
        dwFieldsPresent |= (flag);                     \
    }


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo1(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    UserInfo21 *pSamrUserInfo21 = NULL;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_1 pUserInfo1 = (PUSER_INFO_1)pSource;
    DWORD dwFieldsPresent = 0;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor         = *ppCursor;
        pSamrUserInfo21 = *ppCursor;
    }

    /* last_logon: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_logoff: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_expiry: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* allow_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* force_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1->usri1_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo1->usri1_name,
                       SAMR_FIELD_ACCOUNT_NAME);

    /* full_name: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* home_directory */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1->usri1_home_dir,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo1->usri1_home_dir,
                       SAMR_FIELD_HOME_DIRECTORY);

    /* home_drive: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_script */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1->usri1_script_path,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo1->usri1_script_path,
                       SAMR_FIELD_LOGON_SCRIPT);

    /* profile_path: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* description: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* workstations: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* comment */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1->usri1_comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo1->usri1_comment,
                       SAMR_FIELD_COMMENT);

    /* parameters: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown1: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown2: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown3: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* buf_count: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(UserInfo21, buf_count, pCursor, dwSize, dwSpaceLeft);

    /* buffer: SKIP */
    err = NetAllocBufferByteBlob(&pCursor,
                                 &dwSpaceLeft,
                                 NULL,
                                 0,
                                 &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* rid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* primary_gid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_flags (make sure the normal user account flag is set */
    err = NetAllocBufferAcbFlagsFromUserFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1->usri1_flags | UF_NORMAL_ACCOUNT,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    dwFieldsPresent |= SAMR_FIELD_ACCT_FLAGS;

    /* fields_present: SKIP (this field is set at the end) */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_hours: SKIP */
    err = NetAllocBufferSamrLogonHoursFromNetLogonHours(
                                       &pCursor,
                                       &dwSpaceLeft,
                                       NULL,
                                       &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* bad_password_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* country_code: SKIP */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* code_page: SKIP */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* nt_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* lm_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* password_expired: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown4: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pSamrUserInfo21)
    {
        /*
         * Set fields present field according to results
         * of conversion to samr user info fields
         */
        pSamrUserInfo21->fields_present = dwFieldsPresent;
    }

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo2(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    UserInfo21 *pSamrUserInfo21 = NULL;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_2 pUserInfo2 = (PUSER_INFO_2)pSource;
    DWORD dwFieldsPresent = 0;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor         = *ppCursor;
        pSamrUserInfo21 = *ppCursor;
    }

    /* last_logon: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_logoff: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_expiry */
    err = NetAllocBufferNtTimeFromWinTime(
                                &pCursor,
                                &dwSpaceLeft,
                                pUserInfo2->usri2_acct_expires,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* allow_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* force_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo2->usri2_name,
                       SAMR_FIELD_ACCOUNT_NAME);

    /* full_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* home_directory */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_home_dir,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo2->usri2_home_dir,
                       SAMR_FIELD_HOME_DIRECTORY);

    /* home_drive: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_script */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_script_path,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo2->usri2_script_path,
                       SAMR_FIELD_LOGON_SCRIPT);

    /* profile_path: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* description: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* workstations */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_workstations,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* comment */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo2->usri2_comment,
                       SAMR_FIELD_COMMENT);

    /* parameters */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_parms,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown1: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown2: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown3: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* buf_count: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(UserInfo21, buf_count, pCursor, dwSize, dwSpaceLeft);

    /* buffer: SKIP */
    err = NetAllocBufferByteBlob(&pCursor,
                                 &dwSpaceLeft,
                                 NULL,
                                 0,
                                 &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* rid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* primary_gid */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_flags (make sure the normal user account flag is set */
    err = NetAllocBufferAcbFlagsFromUserFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo2->usri2_flags | UF_NORMAL_ACCOUNT,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    dwFieldsPresent |= SAMR_FIELD_ACCT_FLAGS;

    /* fields_present: SKIP (this field is set at the end) */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_hours: SKIP */
    err = NetAllocBufferSamrLogonHoursFromNetLogonHours(
                                       &pCursor,
                                       &dwSpaceLeft,
                                       NULL,
                                       &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* bad_password_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* country_code */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              pUserInfo2->usri2_country_code,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* code_page */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             pUserInfo2->usri2_code_page,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* nt_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* lm_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* password_expired: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown4: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pSamrUserInfo21)
    {
        /*
         * Set fields present field according to results
         * of conversion to samr user info fields
         */
        pSamrUserInfo21->fields_present = dwFieldsPresent;
    }

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo3(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    UserInfo21 *pSamrUserInfo21 = NULL;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_3 pUserInfo3 = (PUSER_INFO_3)pSource;
    DWORD dwFieldsPresent = 0;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor         = *ppCursor;
        pSamrUserInfo21 = *ppCursor;
    }

    /* last_logon: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_logoff: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_expiry */
    err = NetAllocBufferNtTimeFromWinTime(
                                &pCursor,
                                &dwSpaceLeft,
                                pUserInfo3->usri3_acct_expires,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* allow_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* force_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo3->usri3_name,
                       SAMR_FIELD_ACCOUNT_NAME);

    /* full_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* home_directory */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_home_dir,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo3->usri3_home_dir,
                       SAMR_FIELD_HOME_DIRECTORY);

    /* home_drive: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_script */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_script_path,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo3->usri3_script_path,
                       SAMR_FIELD_LOGON_SCRIPT);

    /* profile_path: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* description: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* workstations */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_workstations,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* comment */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo3->usri3_comment,
                       SAMR_FIELD_COMMENT);

    /* parameters */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_parms,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown1: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown2: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown3: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* buf_count: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(UserInfo21, buf_count, pCursor, dwSize, dwSpaceLeft);

    /* buffer: SKIP */
    err = NetAllocBufferByteBlob(&pCursor,
                                 &dwSpaceLeft,
                                 NULL,
                                 0,
                                 &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* rid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* primary_gid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_flags (make sure the normal user account flag is set */
    err = NetAllocBufferAcbFlagsFromUserFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo3->usri3_flags | UF_NORMAL_ACCOUNT,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    dwFieldsPresent |= SAMR_FIELD_ACCT_FLAGS;

    /* fields_present: SKIP (this field is set at the end) */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_hours: SKIP */
    err = NetAllocBufferSamrLogonHoursFromNetLogonHours(
                                       &pCursor,
                                       &dwSpaceLeft,
                                       NULL,
                                       &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* bad_password_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* country_code */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              pUserInfo3->usri3_country_code,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* code_page */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             pUserInfo3->usri3_code_page,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* nt_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* lm_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* password_expired: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown4: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pSamrUserInfo21)
    {
        /*
         * Set fields present field according to results
         * of conversion to samr user info fields
         */
        pSamrUserInfo21->fields_present = dwFieldsPresent;
    }

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo21FromUserInfo4(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    UserInfo21 *pSamrUserInfo21 = NULL;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_4 pUserInfo4 = (PUSER_INFO_4)pSource;
    DWORD dwFieldsPresent = 0;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor         = *ppCursor;
        pSamrUserInfo21 = *ppCursor;
    }

    /* last_logon: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_logoff: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* last_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_expiry */
    err = NetAllocBufferNtTimeFromWinTime(
                                &pCursor,
                                &dwSpaceLeft,
                                pUserInfo4->usri4_acct_expires,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* allow_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* force_password_change: SKIP */
    err = NetAllocBufferUlong64(&pCursor,
                                &dwSpaceLeft,
                                0,
                                &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo4->usri4_name,
                       SAMR_FIELD_ACCOUNT_NAME);

    /* full_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* home_directory */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_home_dir,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo4->usri4_home_dir,
                       SAMR_FIELD_HOME_DIRECTORY);

    /* home_drive: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_script */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_script_path,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo4->usri4_script_path,
                       SAMR_FIELD_LOGON_SCRIPT);

    /* profile_path: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* description: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* workstations */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_workstations,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* comment */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    SAMR_FIELD_PRESENT(pUserInfo4->usri4_comment,
                       SAMR_FIELD_COMMENT);

    /* parameters */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_parms,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown1: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown2: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown3: SKIP */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   NULL,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* buf_count: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    ALIGN_PTR_IN_BUFFER(UserInfo21, buf_count, pCursor, dwSize, dwSpaceLeft);

    /* buffer: SKIP */
    err = NetAllocBufferByteBlob(&pCursor,
                                 &dwSpaceLeft,
                                 NULL,
                                 0,
                                 &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* rid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* primary_gid: SKIP */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* account_flags (make sure the normal user account flag is set */
    err = NetAllocBufferAcbFlagsFromUserFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo4->usri4_flags | UF_NORMAL_ACCOUNT,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    dwFieldsPresent |= SAMR_FIELD_ACCT_FLAGS;

    /* fields_present: SKIP (this field is set at the end) */
    err = NetAllocBufferDword(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_hours: SKIP */
    err = NetAllocBufferSamrLogonHoursFromNetLogonHours(
                                       &pCursor,
                                       &dwSpaceLeft,
                                       NULL,
                                       &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* bad_password_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* logon_count: SKIP */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* country_code */
    err = NetAllocBufferWord(&pCursor,
                              &dwSpaceLeft,
                              pUserInfo4->usri4_country_code,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* code_page */
    err = NetAllocBufferWord(&pCursor,
                             &dwSpaceLeft,
                             pUserInfo4->usri4_code_page,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* nt_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* lm_password_set: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                              &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* password_expired: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    /* unknown4: SKIP */
    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             0,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pSamrUserInfo21)
    {
        /*
         * Set fields present field according to results
         * of conversion to samr user info fields
         */
        pSamrUserInfo21->fields_present = dwFieldsPresent;
    }

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo26FromUserInfo1(
    PVOID   *ppCursor,
    PDWORD   pdwSpaceLeft,
    PVOID    pSource,
    NetConn *pConn,
    PDWORD   pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    NTSTATUS status = STATUS_SUCCESS;
    PUSER_INFO_1 pUserInfo1 = (PUSER_INFO_1)pSource;

    BAIL_ON_INVALID_PTR(pConn);

    err = NetAllocateSamrUserInfo26FromPassword(
                                   ppCursor,
                                   pdwSpaceLeft,
                                   pUserInfo1->usri1_password,
                                   pConn,
                                   pdwSize);
cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo26FromUserInfo1003(
    PVOID   *ppCursor,
    PDWORD   pdwSpaceLeft,
    PVOID    pSource,
    NetConn *pConn,
    PDWORD   pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    NTSTATUS status = STATUS_SUCCESS;
    PUSER_INFO_1003 pUserInfo1003 = (PUSER_INFO_1003)pSource;

    BAIL_ON_INVALID_PTR(pConn);

    err = NetAllocateSamrUserInfo26FromPassword(
                                   ppCursor,
                                   pdwSpaceLeft,
                                   pUserInfo1003->usri1003_password,
                                   pConn,
                                   pdwSize);
cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo26FromPassword(
    PVOID   *ppCursor,
    PDWORD   pdwSpaceLeft,
    PWSTR    pwszPassword,
    NetConn *pConn,
    PDWORD   pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    NTSTATUS status = STATUS_SUCCESS;
    UserInfo26 *pSamrUserInfo26 = NULL;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    DWORD dwPasswordLen = 0;
    BYTE PasswordBuffer[532] = {0};

    BAIL_ON_INVALID_PTR(pConn);

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor         = *ppCursor;
        pSamrUserInfo26 = *ppCursor;
    }

    if (!pwszPassword)
    {
        err = ERROR_INVALID_PASSWORD;
        BAIL_ON_WINERR_ERROR(err);
    }

    err = LwWc16sLen(pwszPassword,
                     (size_t*)&dwPasswordLen);
    BAIL_ON_WINERR_ERROR(err);

    status = NetEncPasswordEx(PasswordBuffer,
                              pwszPassword,
                              dwPasswordLen,
                              pConn);
    BAIL_ON_NTSTATUS_ERROR(status);

    err = NetAllocBufferFixedBlob(&pCursor,
                                  &dwSpaceLeft,
                                  PasswordBuffer,
                                  sizeof(PasswordBuffer),
                                  &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    err = NetAllocBufferByte(&pCursor,
                             &dwSpaceLeft,
                             dwPasswordLen,
                             &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    memset(PasswordBuffer, 0, sizeof(PasswordBuffer));

    if (err == ERROR_SUCCESS &&
        status != STATUS_SUCCESS)
    {
        err = LwNtStatusToWin32Error(status);
    }

    return err;

error:
    if (pSamrUserInfo26)
    {
        memset(pSamrUserInfo26, 0, sizeof(*pSamrUserInfo26));
    }

    goto cleanup;
}



NTSTATUS PushUserInfoAdd(UserInfo **sinfo, uint32 *slevel, void *ptr,
                         uint32 nlevel, uint32 *parm_err)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo21 *info21 = NULL;
    USER_INFO_X *ninfo = NULL;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ptr);
    /* parm_err is optional and can be NULL */

    if (nlevel < 1 || nlevel > 4) {
        status = STATUS_INVALID_LEVEL;
        goto error;
    }

    ninfo = ptr;

    if (parm_err) {
        *parm_err = 0;
    }


    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 21;
    info21  = &info->info21;

    /* privileges field must be set to USER_PRIV_USER when
       calling NetUserAdd function */
    if (ninfo->priv != USER_PRIV_USER) {
        if (parm_err) {
            *parm_err = (void*)&ninfo->priv - (void*)ninfo;
        }

        status = STATUS_INVALID_PARAMETER;
        goto error;
    }

    info21->fields_present = 0;

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->home_dir, home_directory,
                                 SAMR_FIELD_HOME_DIRECTORY, info);

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->comment, comment,
                                 SAMR_FIELD_COMMENT, info);

    PUSH_ACCOUNT_FLAGS(info21, ninfo->flags, account_flags,
                       SAMR_FIELD_ACCT_FLAGS);

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->script_path, logon_script,
                                 SAMR_FIELD_LOGON_SCRIPT, info);

    *sinfo = info;

cleanup:
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
    goto cleanup;
}


NTSTATUS PushUserInfo0(UserInfo **sinfo, uint32 *slevel, USER_INFO_0 *ninfo)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo21 *info21 = NULL;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ninfo);

    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 21;
    info21 = &info->info21;

    info21->fields_present = 0;

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->usri0_name, account_name,
                                 SAMR_FIELD_ACCOUNT_NAME, info);

    *sinfo = info;

cleanup:
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo8FromUserInfo1011(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_1011 pUserInfo1011 = (PUSER_INFO_1011)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* full_name */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1011->usri1011_full_name,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


static
DWORD
NetAllocateSamrUserInfo13FromUserInfo1007(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_1007 pUserInfo1007 = (PUSER_INFO_1007)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* description */
    err = NetAllocBufferUnicodeStringFromWC16String(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1007->usri1007_comment,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);

    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}



static
DWORD
NetAllocateSamrUserInfo16FromUserInfo1008(
    PVOID  *ppCursor,
    PDWORD  pdwSpaceLeft,
    PVOID   pSource,
    PDWORD  pdwSize
    )
{
    DWORD err = ERROR_SUCCESS;
    PVOID pCursor = NULL;
    DWORD dwSpaceLeft = 0;
    DWORD dwSize = 0;
    PUSER_INFO_1008 pUserInfo1008 = (PUSER_INFO_1008)pSource;

    if (pdwSpaceLeft)
    {
        dwSpaceLeft = *pdwSpaceLeft;
    }

    if (pdwSize)
    {
        dwSize = *pdwSize;
    }

    if (ppCursor)
    {
        pCursor = *ppCursor;
    }

    /* account_flags (make sure the normal user account flag is set */
    err = NetAllocBufferAcbFlagsFromUserFlags(
                                   &pCursor,
                                   &dwSpaceLeft,
                                   pUserInfo1008->usri1008_flags,
                                   &dwSize);
    BAIL_ON_WINERR_ERROR(err);


    if (pdwSpaceLeft)
    {
        *pdwSpaceLeft = dwSpaceLeft;
    }

    if (pdwSize)
    {
        *pdwSize = dwSize;
    }

cleanup:
    return err;

error:
    goto cleanup;
}


NTSTATUS PushUserInfo1(UserInfo **sinfo, uint32 *slevel, USER_INFO_1 *ninfo)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo21 *info21 = NULL;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ninfo);

    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 21;
    info21 = &info->info21;

    info21->fields_present = 0;

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->usri1_home_dir, home_directory,
                                 SAMR_FIELD_HOME_DIRECTORY, info);

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->usri1_comment, comment,
                                 SAMR_FIELD_COMMENT, info);

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->usri1_script_path, logon_script,
                                 SAMR_FIELD_LOGON_SCRIPT, info);

    PUSH_ACCOUNT_FLAGS(info21, ninfo->usri1_flags, account_flags,
                       SAMR_FIELD_ACCT_FLAGS);

    *sinfo = info;

cleanup:
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
    goto cleanup;
}


NTSTATUS PushUserInfo1003(UserInfo **sinfo, uint32 *slevel, USER_INFO_1003 *ninfo,
                          NetConn *conn)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo25 *info25 = NULL;
    wchar16_t *password = NULL;
    size_t password_len = 0;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ninfo);
    BAIL_ON_INVALID_PTR(conn);

    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 25;
    info25 = &info->info25;

    password = wc16sdup(ninfo->usri1003_password);
    BAIL_ON_NO_MEMORY(password);

    password_len = wc16slen(password);
    status = NetEncPasswordEx(info25->password.data, password, password_len, conn);
    BAIL_ON_NTSTATUS_ERROR(status);

    info25->info.fields_present = SAMR_FIELD_PASSWORD;

    *sinfo = info;

cleanup:
    SAFE_FREE(password);
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
    goto cleanup;
}


NTSTATUS PushUserInfo1007(UserInfo **sinfo, uint32 *slevel, USER_INFO_1007 *ninfo)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo21 *info21 = NULL;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ninfo);

    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 21;
    info21 = &info->info21;

    info21->fields_present = 0;

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->usri1007_comment, comment,
                                 SAMR_FIELD_COMMENT, info);

    *sinfo = info;

cleanup:
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
    goto cleanup;
}


NTSTATUS PushUserInfo1008(UserInfo **sinfo, uint32 *slevel, USER_INFO_1008 *ninfo)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo21 *info21 = NULL;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ninfo);

    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 21;
    info21 = &info->info21;

    info21->fields_present = 0;

    PUSH_ACCOUNT_FLAGS(info21, ninfo->usri1008_flags, account_flags,
                       SAMR_FIELD_ACCT_FLAGS);

    *sinfo = info;

cleanup:
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
    goto cleanup;
}


NTSTATUS PushUserInfo1011(UserInfo **sinfo, uint32 *slevel, USER_INFO_1011 *ninfo)
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    UserInfo *info = NULL;
    UserInfo21 *info21 = NULL;

    BAIL_ON_INVALID_PTR(sinfo);
    BAIL_ON_INVALID_PTR(slevel);
    BAIL_ON_INVALID_PTR(ninfo);

    status = NetAllocateMemory((void**)&info, sizeof(UserInfo), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    *slevel = 21;
    info21 = &info->info21;

    info21->fields_present = 0;

    PUSH_UNICODE_STRING_USERINFO(info21, ninfo->usri1011_full_name, full_name,
                                 SAMR_FIELD_FULL_NAME, info);

    *sinfo = info;

cleanup:
    return status;

error:
    if (info) {
        NetFreeMemory((void*)info);
    }

    *sinfo  = NULL;
    *slevel = 0;
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
