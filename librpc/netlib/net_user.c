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
 *        net_user.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Client Interface
 *
 *        NetAPI user and alias (a.k.a. local group) open routines
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


NTSTATUS
NetOpenUser(
    NetConn        *pConn,
    PCWSTR          pwszUsername,
    DWORD           dwAccessMask,
    ACCOUNT_HANDLE *phUser,
    PDWORD          pdwRid
    )
{
    const DWORD dwNumUsers = 1;

    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    handle_t hSamrBinding = NULL;
    DOMAIN_HANDLE hDomain = NULL;
    ACCOUNT_HANDLE hUser = NULL;
    PWSTR ppwszUsernames[1] = {0};
    PDWORD pdwRids = NULL;
    PDWORD pdwTypes = NULL;

    BAIL_ON_INVALID_PTR(pConn);
    BAIL_ON_INVALID_PTR(pwszUsername);
    BAIL_ON_INVALID_PTR(phUser);
    BAIL_ON_INVALID_PTR(pdwRid);

    hSamrBinding = pConn->samr.bind;
    hDomain      = pConn->samr.hDomain;

    ppwszUsernames[0] = wc16sdup(pwszUsername);
    BAIL_ON_NO_MEMORY(ppwszUsernames[0]);

    status = SamrLookupNames(hSamrBinding,
                             hDomain,
                             dwNumUsers,
                             ppwszUsernames,
                             &pdwRids,
                             &pdwTypes,
                             NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = SamrOpenUser(hSamrBinding,
                          hDomain,
                          dwAccessMask,
                          pdwRids[0],
                          &hUser);
    BAIL_ON_NTSTATUS_ERROR(status);

    *pdwRid = pdwRids[0];
    *phUser = hUser;

cleanup:
    if (pdwRids)
    {
        SamrFreeMemory((void*)pdwRids);
    }

    if (pdwTypes)
    {
        SamrFreeMemory((void*)pdwTypes);
    }

    SAFE_FREE(ppwszUsernames[0]);

    return status;

error:
    *pdwRid = 0;
    *phUser = NULL;

    goto cleanup;
}


NTSTATUS
NetOpenAlias(
    NetConn        *pConn,
    PCWSTR          pwszAliasname,
    DWORD           dwAccessMask,
    ACCOUNT_HANDLE *phAlias,
    PDWORD          pdwRid
    )
{
    const DWORD dwNumAliases = 1;

    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    handle_t hSamrBinding = NULL;
    DOMAIN_HANDLE hDomains[2] = {0};
    DOMAIN_HANDLE hDomain = NULL;
    ACCOUNT_HANDLE hAlias = NULL;
    PWSTR ppwszAliasnames[1] = {0};
    PDWORD pdwRids = NULL;
    PDWORD pdwTypes = NULL;
    DWORD dwAliasRid = 0;
    DWORD i = 0;

    BAIL_ON_INVALID_PTR(pConn);
    BAIL_ON_INVALID_PTR(pwszAliasname);
    BAIL_ON_INVALID_PTR(phAlias);
    BAIL_ON_INVALID_PTR(pdwRid);

    hSamrBinding = pConn->samr.bind;
    hDomains[0]  = pConn->samr.hDomain;
    hDomains[1]  = pConn->samr.hBtinDomain;

    ppwszAliasnames[0] = wc16sdup(pwszAliasname);
    BAIL_ON_NO_MEMORY(ppwszAliasnames[0]);

    /*
     * Try to look for alias in host domain first, then in builtin
     */
    for (i = 0; i < sizeof(hDomains)/sizeof(hDomains[0]); i++)
    {
        status = SamrLookupNames(hSamrBinding,
                                 hDomains[i],
                                 dwNumAliases,
                                 ppwszAliasnames,
                                 (PUINT32*)&pdwRids,
                                 (PUINT32*)&pdwTypes,
                                 NULL);
        if (status == STATUS_SUCCESS)
        {
            /*
             * Alias has been found in one of domains so pass
             * that domain handle further down
             */
            hDomain    = hDomains[i];
            dwAliasRid = pdwRids[0];
            break;

        }
        else if (status == STATUS_NONE_MAPPED)
        {
            if (pdwRids)
            {
                SamrFreeMemory((void*)pdwRids);
                pdwRids = NULL;
            }

            if (pdwTypes)
            {
                SamrFreeMemory((void*)pdwTypes);
                pdwTypes = NULL;
            }

            continue;
        }

        /* Catch other possible errors */
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    /* Allow to open alias only if a valid one has been found */
    BAIL_ON_NTSTATUS_ERROR(status);

    status = SamrOpenAlias(hSamrBinding,
                           hDomain,
                           dwAccessMask,
                           dwAliasRid,
                           &hAlias);
    BAIL_ON_NTSTATUS_ERROR(status);

    *pdwRid  = dwAliasRid;
    *phAlias = hAlias;

cleanup:
    SAFE_FREE(ppwszAliasnames[0]);

    if (pdwRids)
    {
        SamrFreeMemory((void*)pdwRids);
    }

    if (pdwTypes)
    {
        SamrFreeMemory((void*)pdwTypes);
    }

    return status;

error:
    *pdwRid  = 0;
    *phAlias = NULL;

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
