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
 *        samr_querysecurity.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        SamrQuerySecurity function
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


NTSTATUS
SamrSrvQuerySecurity(
    IN  handle_t                     hBinding,
    IN  void                        *hObject,
    IN  DWORD                        dwSecurityInfo,
    OUT PSECURITY_DESCRIPTOR_BUFFER *ppSecDescBuf
    )
{
    const wchar_t wszFilterFmt[] = L"%ws='%ws'";

    NTSTATUS ntStatus = STATUS_SUCCESS;
    DWORD dwError = ERROR_SUCCESS;
    PSAMR_GENERIC_CONTEXT pCtx = (PSAMR_GENERIC_CONTEXT)hObject;
    PDOMAIN_CONTEXT pDomCtx = NULL;
    PACCOUNT_CONTEXT pAcctCtx = NULL;
    PWSTR pwszDn = NULL;
    HANDLE hDirectory = NULL;
    PWSTR pwszBaseDn = NULL;
    DWORD dwScope = 0;
    WCHAR wszAttrDn[] = DS_ATTR_DISTINGUISHED_NAME;
    WCHAR wszAttrSecDesc[] = DS_ATTR_SECURITY_DESCRIPTOR;
    size_t sDnLen = 0;
    DWORD dwFilterLen = 0;
    PWSTR pwszFilter = NULL;
    PDIRECTORY_ENTRY pEntries = NULL;
    DWORD dwNumEntries = 0;
    PDIRECTORY_ENTRY pObjectEntry = NULL;
    POCTET_STRING pSecDescBlob = NULL;
    PSECURITY_DESCRIPTOR_BUFFER pSecDescBuf = NULL;

    PWSTR wszAttributes[] = {
        wszAttrSecDesc,
        NULL
    };

    /*
     * Only querying security descriptor on an account is allowed
     */
    if (pCtx->Type == SamrContextAccount)
    {
        pAcctCtx    = (PACCOUNT_CONTEXT)hObject;
        pDomCtx     = pAcctCtx->pDomCtx;
        pwszDn      = pAcctCtx->pwszDn;
        hDirectory  = pDomCtx->pConnCtx->hDirectory;
    }
    else if (pCtx->Type == SamrContextConnect ||
             pCtx->Type == SamrContextDomain)
    {
        ntStatus = STATUS_ACCESS_DENIED;
    }
    else
    {
        ntStatus = STATUS_INVALID_HANDLE;
    }
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    /*
     * 1) READ_CONTROL right is required to read a security descriptor
     * 2) Querying SACL part of the SD is not permitted over rpc
     */
    if (!(pAcctCtx->dwAccessGranted & READ_CONTROL) ||
        dwSecurityInfo & SACL_SECURITY_INFORMATION)
    {
        ntStatus = STATUS_ACCESS_DENIED;
        BAIL_ON_NTSTATUS_ERROR(ntStatus);
    }

    dwError = LwWc16sLen(pwszDn, &sDnLen);
    BAIL_ON_LSA_ERROR(dwError);

    dwFilterLen = ((sizeof(wszAttrDn)/sizeof(wszAttrDn[0])) - 1) +
                  sDnLen +
                  (sizeof(wszFilterFmt)/sizeof(wszFilterFmt[0]));

    dwError = LwAllocateMemory(sizeof(WCHAR) * dwFilterLen,
                               OUT_PPVOID(&pwszFilter));
    BAIL_ON_LSA_ERROR(dwError);

    if (sw16printfw(pwszFilter, dwFilterLen, wszFilterFmt,
                    wszAttrDn,
                    pwszDn) < 0)
    {
        ntStatus = LwErrnoToNtStatus(errno);
        BAIL_ON_NTSTATUS_ERROR(ntStatus);
    }

    dwError = DirectorySearch(hDirectory,
                              pwszBaseDn,
                              dwScope,
                              pwszFilter,
                              wszAttributes,
                              FALSE,
                              &pEntries,
                              &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        ntStatus = STATUS_INVALID_HANDLE;
    }
    else if (dwNumEntries > 1)
    {
        ntStatus = STATUS_INTERNAL_ERROR;
    }
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    pObjectEntry = &(pEntries[0]);

    dwError = DirectoryGetEntryAttrValueByName(
                              pObjectEntry,
                              wszAttrSecDesc,
                              DIRECTORY_ATTR_TYPE_NT_SECURITY_DESCRIPTOR,
                              &pSecDescBlob);
    BAIL_ON_LSA_ERROR(dwError);

    ntStatus = SamrSrvAllocateSecDescBuffer(
                              &pSecDescBuf,
                              (SECURITY_INFORMATION)dwSecurityInfo,
                              pSecDescBlob);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    *ppSecDescBuf = pSecDescBuf;

cleanup:
    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    LW_SAFE_FREE_MEMORY(pwszFilter);

    return ntStatus;

error:
    if (pSecDescBuf)
    {
        if (pSecDescBuf->pBuffer)
        {
            SamrSrvFreeMemory(pSecDescBuf->pBuffer);
        }

        SamrSrvFreeMemory(pSecDescBuf);
    }

    *ppSecDescBuf = NULL;

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
