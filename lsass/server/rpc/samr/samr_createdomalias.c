/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software    2004-2009
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
 *        samr_createdomalias2.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        SamrCreateDomAlias function
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


NTSTATUS
SamrSrvCreateDomAlias(
    /* [in] */ handle_t hBinding,
    /* [in] */ DOMAIN_HANDLE hDomain,
    /* [in] */ UnicodeString *alias_name,
    /* [in] */ uint32 access_mask,
    /* [out] */ ACCOUNT_HANDLE *hAlias,
    /* [out] */ uint32 *rid
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PDOMAIN_CONTEXT pDomCtx = NULL;
    PWSTR pwszAliasName = NULL;
    UnicodeStringEx Name;
    uint32 ulAccessGranted = 0;

    pDomCtx = (PDOMAIN_CONTEXT)hDomain;

    if (pDomCtx == NULL || pDomCtx->Type != SamrContextDomain) {
        status = STATUS_INVALID_HANDLE;
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    status = SamrSrvGetFromUnicodeString(&pwszAliasName,
                                         alias_name,
                                         pDomCtx);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = SamrSrvInitUnicodeStringEx(&Name,
                                        pwszAliasName,
                                        pDomCtx);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = SamrSrvCreateAccount(hBinding,
                                  hDomain,
                                  &Name,
                                  "group",
                                  0,
                                  access_mask,
                                  hAlias,
                                  &ulAccessGranted,
                                  rid);

cleanup:
    if (pwszAliasName) {
        SamrSrvFreeMemory(pwszAliasName);
    }

    SamrSrvFreeUnicodeString(&Name);

    return status;

error:
    *hAlias = NULL;
    *rid    = 0;
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
