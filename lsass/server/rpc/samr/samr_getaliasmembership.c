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
 *        samr_getaliasmembership.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        SamrGetAliasMembership function
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


static
VOID
FreeSid(
    PVOID pData,
    PVOID pUserData
    );


static
VOID
SetRidsArray(
    PVOID pData,
    PVOID pUserData
    );


NTSTATUS SamrSrvGetAliasMembership(
    /* [in] */ handle_t hBinding,
    /* [in] */ DOMAIN_HANDLE hDomain,
    /* [in] */ SidArray *pSids,
    /* [out] */ Ids *pRids
    )
{
    wchar_t wszFilterFmt[] = L"%ws='%ws'";
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PDOMAIN_CONTEXT pDomCtx = NULL;
    DWORD dwError = 0;
    DWORD dwDnNum = 0;
    PWSTR *ppwszDn = NULL;
    PWSTR pwszSid = NULL;
    PSID pSid = NULL;
    DWORD i = 0;
    DWORD iGroup = 0;
    PDLINKEDLIST pSidList = NULL;
    HANDLE hDirectory = NULL;
    DWORD dwScope = 0;
    PWSTR pwszBase = NULL;
    size_t sSidStrLen = 0;
    DWORD dwFilterLen = 0;
    PWSTR pwszFilter = NULL;
    WCHAR wszAttrObjectSid[] = DS_ATTR_OBJECT_SID;
    WCHAR wszAttrDn[] = DS_ATTR_DISTINGUISHED_NAME;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwEntriesNum = 0;
    PDIRECTORY_ENTRY pLocalGroupEntries = NULL;
    DWORD dwLocalGroupsNum = 0;
    PWSTR pwszDn = NULL;
    DWORD dwSidCount = 0;
    Ids Rids = {0};

    PWSTR wszMemberAttributes[] = {
        wszAttrDn,
        NULL
    };

    PWSTR wszLocalGroupAttributes[] = {
        wszAttrObjectSid,
        NULL
    };

    pDomCtx    = (PDOMAIN_CONTEXT)hDomain;
    hDirectory = pDomCtx->pConnCtx->hDirectory;
    dwDnNum    = pSids->num_sids;

    dwError = LwAllocateMemory(sizeof(PWSTR) * dwDnNum,
                               OUT_PPVOID(&ppwszDn));
    BAIL_ON_LSA_ERROR(dwError);

    for (i = 0; i < pSids->num_sids; i++)
    {
        ntStatus = RtlAllocateWC16StringFromSid(&pwszSid,
                                              pSids->sids[i].sid);
        BAIL_ON_NTSTATUS_ERROR(ntStatus);

        dwError = LwWc16sLen(pwszSid, &sSidStrLen);
        BAIL_ON_LSA_ERROR(dwError);

        dwFilterLen = ((sizeof(wszAttrObjectSid)/sizeof(WCHAR)) - 1) +
                      sSidStrLen +
                      (sizeof(wszFilterFmt)/sizeof(wszFilterFmt[0]));

        dwError = LwAllocateMemory(sizeof(WCHAR) * dwFilterLen,
                                   OUT_PPVOID(&pwszFilter));
        BAIL_ON_LSA_ERROR(dwError);

        sw16printfw(pwszFilter, dwFilterLen, wszFilterFmt,
                    wszAttrObjectSid,
                    pwszSid);

        dwError = DirectorySearch(hDirectory,
                                  pwszBase,
                                  dwScope,
                                  pwszFilter,
                                  wszMemberAttributes,
                                  FALSE,
                                  &pEntry,
                                  &dwEntriesNum);
        BAIL_ON_LSA_ERROR(dwError);

        if (dwEntriesNum > 1)
        {
            ntStatus = STATUS_INTERNAL_ERROR;
            BAIL_ON_NTSTATUS_ERROR(ntStatus);
        }
        else if (dwEntriesNum == 1)
        {
            dwError = DirectoryGetEntryAttrValueByName(
                                      pEntry,
                                      wszAttrDn,
                                      DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                                      &pwszDn);
            BAIL_ON_LSA_ERROR(dwError);

            if (pwszDn == NULL)
            {
                ntStatus = STATUS_INTERNAL_ERROR;
                BAIL_ON_NTSTATUS_ERROR(ntStatus);
            }

            dwError = LwAllocateWc16String(&ppwszDn[i],
                                           pwszDn);
            BAIL_ON_LSA_ERROR(ntStatus);
        }

        if (pEntry)
        {
            DirectoryFreeEntries(pEntry, dwEntriesNum);
            pEntry = NULL;
        }

        LW_SAFE_FREE_MEMORY(pwszFilter);
        RTL_FREE(&pwszSid);

        pwszFilter = NULL;
        pwszSid    = NULL;
    }

    for (i = 0; i < dwDnNum; i++)
    {
        pwszDn = ppwszDn[i];

        dwError = DirectoryGetMemberships(
                                  hDirectory,
                                  pwszDn,
                                  wszLocalGroupAttributes,
                                  &pLocalGroupEntries,
                                  &dwLocalGroupsNum);
        BAIL_ON_LSA_ERROR(dwError);

        for (iGroup = 0; iGroup < dwLocalGroupsNum; iGroup++)
        {
            dwError = DirectoryGetEntryAttrValueByName(
                                  pLocalGroupEntries,
                                  wszAttrObjectSid,
                                  DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                                  &pwszSid);
            BAIL_ON_LSA_ERROR(dwError);

            if (pwszSid == NULL)
            {
                ntStatus = STATUS_INTERNAL_ERROR;
                BAIL_ON_NTSTATUS_ERROR(ntStatus);
            }

            ntStatus = RtlAllocateSidFromWC16String(&pSid, pwszSid);
            BAIL_ON_NTSTATUS_ERROR(ntStatus);

            if (RtlIsPrefixSid(pDomCtx->pDomainSid, pSid))
            {
                dwError = LsaDLinkedListAppend(&pSidList, pSid);
                BAIL_ON_LSA_ERROR(dwError);
            }
        }

        if (pLocalGroupEntries)
        {
            DirectoryFreeEntries(pLocalGroupEntries,
                                 dwLocalGroupsNum);
        }
    }

    dwSidCount = LsaDLinkedListLength(pSidList);

    ntStatus = SamrSrvAllocateMemory(OUT_PPVOID(&Rids.ids),
                                   sizeof(Rids.ids[0]) * dwSidCount);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    LsaDLinkedListForEach(pSidList, SetRidsArray, (PVOID)&Rids);

cleanup:
    if (pEntry)
    {
        DirectoryFreeEntries(pEntry, dwEntriesNum);
    }

    if (pLocalGroupEntries)
    {
        DirectoryFreeEntries(pLocalGroupEntries,
                             dwLocalGroupsNum);
    }

    LW_SAFE_FREE_MEMORY(pwszFilter);
    RTL_FREE(&pwszSid);

    LsaDLinkedListForEach(pSidList, FreeSid, NULL);
    LsaDLinkedListFree(pSidList);

    if (ntStatus == STATUS_SUCCESS &&
        dwError != ERROR_SUCCESS)
    {
        ntStatus = LwWin32ErrorToNtStatus(dwError);
    }

    return ntStatus;

error:
    goto cleanup;
}


static
VOID
FreeSid(
    PVOID pData,
    PVOID pUserData
    )
{
    PSID pSid = (PSID)pData;
    RTL_FREE(&pSid);
}


static
VOID
SetRidsArray(
    PVOID pData,
    PVOID pUserData
    )
{
    PSID pAliasSid = (PSID)pData;
    Ids *pRids = (Ids*)pUserData;
    DWORD i = pRids->count;

    pRids->ids[i] = pAliasSid->SubAuthority[pAliasSid->SubAuthorityCount - 1];
    pRids->count  = ++i;
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
