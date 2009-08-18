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
 *        lsa_lookupnames3.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        LsaLookupNames3 function
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"

#define SET_LOOKUP_STATUS(lookup, status)                \
    switch ((lookup)) {                                  \
    case STATUS_SUCCESS:                                 \
        (lookup) = (status);                             \
        break;                                           \
    case STATUS_NONE_MAPPED:                             \
        if ((status) == STATUS_SUCCESS ||                \
            (status) == LW_STATUS_SOME_NOT_MAPPED)            \
        {                                                \
            (lookup) = LW_STATUS_SOME_NOT_MAPPED;             \
        }                                                \
    case LW_STATUS_SOME_NOT_MAPPED:                           \
        (lookup) = LW_STATUS_SOME_NOT_MAPPED;                 \
    }



NTSTATUS
LsaSrvLookupNames3(
    handle_t b,
    POLICY_HANDLE hPolicy,
    uint32 num_names,
    UnicodeStringEx *names,
    RefDomainList **domains,
    TranslatedSidArray3 *sids,
    uint16 level,
    uint32 *count,
    uint32 unknown1,
    uint32 unknown2
    )
{
    const DWORD dwSamrConnAccess = SAMR_ACCESS_CONNECT_TO_SERVER;
    const DWORD dwPolicyAccessMask = LSA_ACCESS_LOOKUP_NAMES_SIDS;

    NTSTATUS status = STATUS_SUCCESS;
    NTSTATUS lookup_status = STATUS_SUCCESS;
    DWORD dwError = 0;
    RPCSTATUS rpcstatus = 0;
    PPOLICY_CONTEXT pPolCtx = NULL;
    HANDLE hDirectory = NULL;
    PSTR pszSamrLpcSocketPath = NULL;
    handle_t hSamrBinding = NULL;
    handle_t hLsaBinding = NULL;
    PIO_ACCESS_TOKEN pAccessToken = NULL;
    CHAR szHostname[64];
    PolicyHandle hConn;
    PolicyHandle hDcPolicy;
    SAMR_DOMAIN SamrDomain;
    SAMR_DOMAIN SamrLocalDomain;
    SAMR_DOMAIN SamrBuiltinDomain;
    PSAMR_DOMAIN pSamrDomain = NULL;
    PWSTR pwszSystemName = NULL;
    PACCOUNT_NAMES pDomainAccounts = NULL;
    PACCOUNT_NAMES pLocalAccounts = NULL;
    PACCOUNT_NAMES pBuiltinAccounts = NULL;
    PACCOUNT_NAMES pOtherAccounts = NULL;
    PSTR pszDomainFqdn = NULL;
    PSTR pszDcName = NULL;
    PWSTR pwszDcName = NULL;
    RefDomainList *pRemoteDomains = NULL;
    TranslatedSid3 *pRemoteSids = NULL;
    DWORD dwRemoteSidsCount = 0;
    DWORD *dwRids = NULL;
    DWORD *dwTypes = NULL;
    DWORD dwCount = 0;
    DWORD *dwLocalRids = NULL;
    DWORD *dwLocalTypes = NULL;
    DWORD dwLocalCount = 0;
    DWORD *dwBuiltinRids = NULL;
    DWORD *dwBuiltinTypes = NULL;
    DWORD dwBuiltinCount = 0;
    DWORD i = 0;
    DWORD dwDomIndex = 0;
    DWORD dwLocalDomIndex = 0;
    DWORD dwBuiltinDomIndex = 0;
    LsaDomainInfo *pLocalDomainInfo = NULL;
    LsaDomainInfo *pBuiltinDomainInfo = NULL;
    TranslatedSidArray3 SidArray = {0};
    RefDomainList *pDomains = NULL;

    memset(&SamrDomain, 0, sizeof(SamrDomain));
    memset(szHostname, 0, sizeof(szHostname));

    pPolCtx = (PPOLICY_CONTEXT)hPolicy;

    if (pPolCtx == NULL || pPolCtx->Type != LsaContextPolicy) {
        status = STATUS_INVALID_HANDLE;
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    hDirectory = pPolCtx->hDirectory;

    status = LsaSrvAllocateMemory((void**)&(SidArray.sids),
                                  sizeof(*SidArray.sids) * num_names);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory((void**)&pDomains,
                                  sizeof(*pDomains));
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory((void**)&(pDomains->domains),
                                  sizeof(*pDomains->domains) * (num_names + 2));
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory((PVOID*)&pSamrDomain,
                                   sizeof(*pSamrDomain) * (num_names + 2));
    BAIL_ON_NTSTATUS_ERROR(status);

    pPolCtx->dwSamrDomainsNum = num_names + 2;
    pPolCtx->pSamrDomain      = pSamrDomain;

    dwError = gethostname(szHostname, sizeof(szHostname));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaSrvConfigGetSamrLpcSocketPath(&pszSamrLpcSocketPath);
    BAIL_ON_LSA_ERROR(dwError);

    rpcstatus = InitSamrBindingFull(&hSamrBinding,
                                    "ncalrpc",
                                    szHostname,
                                    pszSamrLpcSocketPath,
                                    NULL,
                                    NULL,
                                    NULL);
    if (rpcstatus) {
        dwError = LW_ERROR_RPC_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LsaMbsToWc16s((PSTR)szHostname, &pwszSystemName);
    BAIL_ON_LSA_ERROR(dwError);

    status = SamrConnect2(hSamrBinding,
                          pwszSystemName,
                          dwSamrConnAccess,
                          &hConn);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvLookupDomainsByAccountName(pPolCtx,
                                              names,
                                              num_names,
                                              hSamrBinding,
                                              &hConn,
                                              &pDomainAccounts,
                                              &pLocalAccounts,
                                              &pBuiltinAccounts,
                                              &pOtherAccounts);
    BAIL_ON_NTSTATUS_ERROR(status);

    if (pDomainAccounts->dwCount) {
        dwError = LWNetGetCurrentDomain(&pszDomainFqdn);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = LWNetGetDomainController(pszDomainFqdn,
                                           &pszDcName);
        BAIL_ON_LSA_ERROR(dwError);

        status = LwIoGetThreadAccessToken(&pAccessToken);
        BAIL_ON_NTSTATUS_ERROR(status);

        rpcstatus = InitLsaBindingDefault(&hLsaBinding,
                                          pszDcName,
                                          pAccessToken);
        if (rpcstatus) {
            dwError = LW_ERROR_RPC_ERROR;
            BAIL_ON_LSA_ERROR(dwError);
        }

        dwError = LsaMbsToWc16s(pszDcName, &pwszDcName);
        BAIL_ON_LSA_ERROR(dwError);

        status = LsaOpenPolicy2(hLsaBinding,
                                pwszDcName,
                                NULL,
                                dwPolicyAccessMask,
                                &hDcPolicy);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = LsaLookupNames3(hLsaBinding,
                                 &hDcPolicy,
                                 pDomainAccounts->dwCount,
                                 pDomainAccounts->ppwszNames,
                                 &pRemoteDomains,
                                 &pRemoteSids,
                                 level,
                                 &dwRemoteSidsCount);

        if (status != STATUS_SUCCESS &&
            status != LW_STATUS_SOME_NOT_MAPPED &&
            status != STATUS_NONE_MAPPED) {
            BAIL_ON_NTSTATUS_ERROR(status);
        }

        lookup_status = status;

        for (i = 0; i < pRemoteDomains->count; i++) {
            LsaDomainInfo *pDomInfo = NULL;
            LsaDomainInfo *pInfo = NULL;

            dwDomIndex = pDomains->count;
            pDomInfo   = &(pRemoteDomains->domains[dwDomIndex]);
            pInfo      = &(pDomains->domains[dwDomIndex]);

            status = LsaSrvDuplicateUnicodeStringEx(&pInfo->name,
                                                    &pDomInfo->name);
            BAIL_ON_NTSTATUS_ERROR(status);

            status = LsaSrvDuplicateSid(&pInfo->sid, pDomInfo->sid);
            BAIL_ON_NTSTATUS_ERROR(status);

            pDomains->count  = (++dwDomIndex);
        }

        for (i = 0; i < dwRemoteSidsCount ; i++) {
            TranslatedSid3 *pRemoteSid = &(pRemoteSids[i]);
            TranslatedSid3 *pSid = &(SidArray.sids[i]);
            PSID pAcctSid = NULL;

            status = LsaSrvDuplicateSid(&pAcctSid, pRemoteSid->sid);
            BAIL_ON_NTSTATUS_ERROR(status);

            pSid->type     = pRemoteSid->type;
            pSid->index    = pRemoteSid->index;
            pSid->unknown1 = pRemoteSid->unknown1;
            pSid->sid      = pAcctSid;
        }

        SidArray.count += dwRemoteSidsCount;

        if (pRemoteDomains) {
            LsaRpcFreeMemory(pRemoteDomains);
        }

        if (pRemoteSids) {
            LsaRpcFreeMemory(pRemoteSids);
        }

        status = LsaClose(hLsaBinding, &hDcPolicy);
        BAIL_ON_NTSTATUS_ERROR(status);

        FreeLsaBinding(&hLsaBinding);
    }

    if (pLocalAccounts->dwCount) {
        dwLocalDomIndex  = pDomains->count;
        pLocalDomainInfo = &(pDomains->domains[dwLocalDomIndex]);

        status = LsaSrvGetLocalSamrDomain(pPolCtx,
                                          FALSE,    /* !BUILTIN */
                                          &SamrDomain);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = LsaSrvInitUnicodeStringEx(&pLocalDomainInfo->name,
                                           SamrDomain.pwszName);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = LsaSrvDuplicateSid(&pLocalDomainInfo->sid,
                                    SamrDomain.pSid);
        BAIL_ON_NTSTATUS_ERROR(status);

        dwRids  = NULL;
        dwTypes = NULL;
        dwCount = 0;

        status = SamrLookupNames(hSamrBinding,
                                 &SamrDomain.hDomain,
                                 pLocalAccounts->dwCount,
                                 pLocalAccounts->ppwszNames,
                                 &dwRids,
                                 &dwTypes,
                                 &dwCount);
        if (status != STATUS_SUCCESS &&
            status != LW_STATUS_SOME_NOT_MAPPED &&
            status != STATUS_NONE_MAPPED) {
            BAIL_ON_NTSTATUS_ERROR(status);
        }

        SET_LOOKUP_STATUS(lookup_status, status);

        for (i = 0; i < dwCount; i++) {
            TranslatedSid3 *pSid = &(SidArray.sids[i + SidArray.count]);
            PSID pDomainSid = SamrDomain.pSid;
            PSID pAcctSid = NULL;

            status = LsaSrvSidAppendRid(&pAcctSid, pDomainSid, dwRids[i]);
            BAIL_ON_NTSTATUS_ERROR(status);

            pSid->type     = dwTypes[i];
            pSid->sid      = pAcctSid;
            pSid->index    = dwLocalDomIndex;
            pSid->unknown1 = 0;
        }

        pDomains->count  = dwLocalDomIndex + 1;
        SidArray.count   += dwCount;
    }

    if (pBuiltinAccounts->dwCount) {
        dwBuiltinDomIndex  = pDomains->count;
        pBuiltinDomainInfo = &(pDomains->domains[dwBuiltinDomIndex]);

        status = LsaSrvGetLocalSamrDomain(pPolCtx,
                                          TRUE,      /* BUILTIN */
                                          &SamrDomain);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = LsaSrvInitUnicodeStringEx(&pBuiltinDomainInfo->name,
                                           SamrDomain.pwszName);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = LsaSrvDuplicateSid(&pBuiltinDomainInfo->sid,
                                    SamrDomain.pSid);
        BAIL_ON_NTSTATUS_ERROR(status);

        dwRids  = NULL;
        dwTypes = NULL;
        dwCount = 0;

        status = SamrLookupNames(hSamrBinding,
                                 &SamrDomain.hDomain,
                                 pBuiltinAccounts->dwCount,
                                 pBuiltinAccounts->ppwszNames,
                                 &dwRids,
                                 &dwTypes,
                                 &dwCount);
        if (status != STATUS_SUCCESS &&
            status != LW_STATUS_SOME_NOT_MAPPED &&
            status != STATUS_NONE_MAPPED) {
            BAIL_ON_NTSTATUS_ERROR(status);
        }

        SET_LOOKUP_STATUS(lookup_status, status);

        for (i = 0; i < dwCount; i++) {
            TranslatedSid3 *pSid = &(SidArray.sids[i + SidArray.count]);
            PSID pDomainSid = SamrDomain.pSid;
            PSID pAcctSid = NULL;

            status = LsaSrvSidAppendRid(&pAcctSid, pDomainSid, dwRids[i]);
            BAIL_ON_NTSTATUS_ERROR(status);

            pSid->type     = dwTypes[i];
            pSid->sid      = pAcctSid;
            pSid->index    = dwBuiltinDomIndex;
            pSid->unknown1 = 0;
        }

        pDomains->count = dwBuiltinDomIndex + 1;
        SidArray.count  += dwCount;
    }

    if (pOtherAccounts->dwCount)
    {
        if (pLocalDomainInfo == NULL)
        {
            dwLocalDomIndex  = pDomains->count;
            pLocalDomainInfo = &(pDomains->domains[dwLocalDomIndex]);

            status = LsaSrvGetLocalSamrDomain(pPolCtx,
                                              FALSE,    /* !BUILTIN */
                                              &SamrLocalDomain);
            BAIL_ON_NTSTATUS_ERROR(status);

            status = LsaSrvInitUnicodeStringEx(&pLocalDomainInfo->name,
                                               SamrLocalDomain.pwszName);
            BAIL_ON_NTSTATUS_ERROR(status);

            status = LsaSrvDuplicateSid(&pLocalDomainInfo->sid,
                                        SamrLocalDomain.pSid);
            BAIL_ON_NTSTATUS_ERROR(status);

            pDomains->count = dwLocalDomIndex + 1;
        }

        if (pBuiltinDomainInfo == NULL)
        {
            dwBuiltinDomIndex  = pDomains->count;
            pBuiltinDomainInfo = &(pDomains->domains[dwBuiltinDomIndex]);

            status = LsaSrvGetLocalSamrDomain(pPolCtx,
                                              TRUE,      /* BUILTIN */
                                              &SamrBuiltinDomain);
            BAIL_ON_NTSTATUS_ERROR(status);

            status = LsaSrvInitUnicodeStringEx(&pBuiltinDomainInfo->name,
                                               SamrBuiltinDomain.pwszName);
            BAIL_ON_NTSTATUS_ERROR(status);

            status = LsaSrvDuplicateSid(&pBuiltinDomainInfo->sid,
                                        SamrBuiltinDomain.pSid);
            BAIL_ON_NTSTATUS_ERROR(status);

            pDomains->count = dwBuiltinDomIndex + 1;
        }

        status = SamrLookupNames(hSamrBinding,
                                 &SamrLocalDomain.hDomain,
                                 pOtherAccounts->dwCount,
                                 pOtherAccounts->ppwszNames,
                                 &dwLocalRids,
                                 &dwLocalTypes,
                                 &dwCount);
        if (status == LW_STATUS_SOME_NOT_MAPPED ||
            status == STATUS_NONE_MAPPED)
        {
            status = SamrLookupNames(hSamrBinding,
                                     &SamrBuiltinDomain.hDomain,
                                     pOtherAccounts->dwCount,
                                     pOtherAccounts->ppwszNames,
                                     &dwBuiltinRids,
                                     &dwBuiltinTypes,
                                     &dwCount);
            if (status != STATUS_SUCCESS &&
                status != LW_STATUS_SOME_NOT_MAPPED &&
                status != STATUS_NONE_MAPPED)
            {
                BAIL_ON_NTSTATUS_ERROR(status);
            }

        }
        else if (status != STATUS_SUCCESS)
        {
            BAIL_ON_NTSTATUS_ERROR(status);
        }

        for (i = 0; i < dwCount; i++)
        {
            TranslatedSid3 *pSid = &(SidArray.sids[i + SidArray.count]);
            PSID pLocalDomainSid = SamrLocalDomain.pSid;
            PSID pBuiltinDomainSid = SamrBuiltinDomain.pSid;
            PSID pAcctSid = NULL;

            if (dwLocalTypes &&
                dwLocalTypes[i] != SID_TYPE_UNKNOWN)
            {
                status = LsaSrvSidAppendRid(&pAcctSid,
                                            pLocalDomainSid,
                                            dwLocalRids[i]);
                BAIL_ON_NTSTATUS_ERROR(status);

                pSid->type     = dwLocalTypes[i];
                pSid->sid      = pAcctSid;
                pSid->index    = dwLocalDomIndex;
                pSid->unknown1 = 0;

            }
            else
            {
                status = LsaSrvSidAppendRid(&pAcctSid,
                                            pBuiltinDomainSid,
                                            dwBuiltinRids[i]);
                BAIL_ON_NTSTATUS_ERROR(status);

                pSid->type     = dwBuiltinTypes[i];
                pSid->sid      = pAcctSid;
                pSid->index    = dwBuiltinDomIndex;
                pSid->unknown1 = 0;
            }
        }

        SidArray.count += dwCount;
    }

    status = SamrClose(hSamrBinding, &hConn);
    BAIL_ON_NTSTATUS_ERROR(status);

    /* windows seems to set max_size to multiple of 32 */
    pDomains->max_size = ((pDomains->count / 32) + 1) * 32;

    *domains    = pDomains;
    sids->count = SidArray.count;
    sids->sids  = SidArray.sids;
    *count      = SidArray.count;

cleanup:
    if (hSamrBinding) {
        FreeSamrBinding(&hSamrBinding);
    }

    if (pwszSystemName) {
        LW_SAFE_FREE_MEMORY(pwszSystemName);
    }

    if (pszDomainFqdn) {
        LWNetFreeString(pszDomainFqdn);
    }

    if (pszDcName) {
        LWNetFreeString(pszDcName);
    }

    if (pDomainAccounts) {
        LsaSrvFreeAccountNames(pDomainAccounts);
    }

    if (pLocalAccounts) {
        LsaSrvFreeAccountNames(pLocalAccounts);
    }

    if (pBuiltinAccounts) {
        LsaSrvFreeAccountNames(pBuiltinAccounts);
    }

    if (pOtherAccounts) {
        LsaSrvFreeAccountNames(pOtherAccounts);
    }

    if (dwRids) {
        SamrFreeMemory(dwRids);
    }

    if (dwTypes) {
        SamrFreeMemory(dwTypes);
    }

    if (dwLocalRids) {
        SamrFreeMemory(dwLocalRids);
    }

    if (dwLocalTypes) {
        SamrFreeMemory(dwLocalTypes);
    }

    if (dwBuiltinRids) {
        SamrFreeMemory(dwBuiltinRids);
    }

    if (dwBuiltinTypes) {
        SamrFreeMemory(dwBuiltinTypes);
    }

    LW_SAFE_FREE_STRING(pszSamrLpcSocketPath);

    if (status == STATUS_SUCCESS) {
        status = lookup_status;
    }

    return status;

error:
    if (pDomains) {
        LsaSrvFreeMemory(pDomains);
    }

    if (SidArray.sids) {
        LsaSrvFreeMemory(SidArray.sids);
    }

    *domains    = NULL;
    sids->count = 0;
    sids->sids  = NULL;
    *count      = 0;
    goto cleanup;

}


NTSTATUS
LsaSrvParseAccountName(
    PWSTR pwszName,
    PWSTR *ppwszDomainName,
    PWSTR *ppwszAcctName
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    DWORD dwError = 0;
    PWSTR pwszCursor = NULL;
    PWSTR pwszDomainName = NULL;
    DWORD dwDomainNameLen = 0;
    PWSTR pwszAcctName = NULL;
    DWORD dwAcctNameLen = 0;

    pwszCursor = pwszName;

    while ((*pwszCursor) &&
           (*pwszCursor) != (WCHAR)'\\') pwszCursor++;

    if ((*pwszCursor) == (WCHAR)'\\') {
        dwDomainNameLen = (DWORD)(pwszCursor - pwszName);
        status = LsaSrvAllocateMemory((PVOID*)&pwszDomainName,
                                       (dwDomainNameLen + 1) * sizeof(WCHAR));
        BAIL_ON_NTSTATUS_ERROR(status);

        wc16sncpy(pwszDomainName, pwszName, dwDomainNameLen);
        pwszCursor++;

    } else {
        pwszCursor = pwszName;
    }

    dwAcctNameLen = wc16slen(pwszCursor);
    status = LsaSrvAllocateMemory((void**)&pwszAcctName,
                                   (dwAcctNameLen + 1) * sizeof(WCHAR));
    BAIL_ON_NTSTATUS_ERROR(status);

    wc16sncpy(pwszAcctName, pwszCursor, dwAcctNameLen);

    *ppwszDomainName = pwszDomainName;
    *ppwszAcctName   = pwszAcctName;

cleanup:
    return status;

error:
    if (pwszDomainName) {
        LsaSrvFreeMemory(pwszDomainName);
    }

    if (pwszAcctName) {
        LsaSrvFreeMemory(pwszAcctName);
    }

    *ppwszDomainName = NULL;
    *ppwszAcctName = NULL;
    goto cleanup;
}


NTSTATUS
LsaSrvGetSamrDomain(
    PPOLICY_CONTEXT pPolCtx,
    PWSTR pwszDomainName,
    PSAMR_DOMAIN pSamrDomain
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    DWORD i = 0;

    for (i = 0; i < pPolCtx->dwSamrDomainsNum; i++) {
        PSAMR_DOMAIN pDomain = &(pPolCtx->pSamrDomain[i]);

        if (wc16scmp(pDomain->pwszName, pwszDomainName) == 0) {
            pSamrDomain->pwszName = pDomain->pwszName;
            pSamrDomain->pSid     = pDomain->pSid;
            pSamrDomain->bLocal   = pDomain->bLocal;
            pSamrDomain->hDomain  = pDomain->hDomain;

            goto cleanup;
        }
    }

    memset(pSamrDomain, 0, sizeof(*pSamrDomain));
    status = STATUS_NO_SUCH_DOMAIN;

cleanup:
    return status;
}


NTSTATUS
LsaSrvSetSamrDomain(
    PPOLICY_CONTEXT pPolCtx,
    PSAMR_DOMAIN pSamrDomain
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    DWORD i = 0;
    PSAMR_DOMAIN pDomain = NULL;

    for (i = 0;
         i < pPolCtx->dwSamrDomainsNum && pPolCtx->pSamrDomain[i].pwszName;
         i++)
    {
        pDomain = &(pPolCtx->pSamrDomain[i]);

        if (wc16scmp(pDomain->pwszName, pSamrDomain->pwszName) == 0) {
            goto cleanup;
        }
    }

    pDomain = &(pPolCtx->pSamrDomain[i]);

    status = LsaSrvDuplicateWC16String(&(pDomain->pwszName),
                                       pSamrDomain->pwszName);
    BAIL_ON_NTSTATUS_ERROR(status);

    if (pSamrDomain->pSid) {
        status = LsaSrvDuplicateSid(&pDomain->pSid,
                                    pSamrDomain->pSid);
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    pDomain->bLocal  = pSamrDomain->bLocal;
    pDomain->hDomain = pSamrDomain->hDomain;

cleanup:
    return status;

error:
    if (i < pPolCtx->dwSamrDomainsNum) {
        pDomain = &(pPolCtx->pSamrDomain[i]);

        if (pDomain->pwszName) {
            LsaSrvFreeMemory(pDomain->pwszName);
            pDomain->pwszName = NULL;
        }

        if (pDomain->pSid) {
            LsaSrvFreeMemory(pDomain->pSid);
            pDomain->pSid = NULL;
        }
    }

    goto cleanup;
}


NTSTATUS
LsaSrvGetLocalSamrDomain(
    PPOLICY_CONTEXT pPolCtx,
    BOOLEAN bBuiltin,
    PSAMR_DOMAIN pSamrDomain
    )
{
    CHAR szBuiltinName[] = "BUILTIN";
    NTSTATUS status = STATUS_SUCCESS;
    DWORD dwError = 0;
    DWORD i = 0;
    PWSTR pwszBuiltinName = NULL;
    BOOLEAN bIsBuiltin = FALSE;

    dwError = LsaMbsToWc16s(szBuiltinName, &pwszBuiltinName);
    BAIL_ON_LSA_ERROR(dwError);

    for (i = 0; i < pPolCtx->dwSamrDomainsNum; i++) {
        PSAMR_DOMAIN pDomain = &(pPolCtx->pSamrDomain[i]);

        if (pDomain->bLocal) {
            bIsBuiltin = (!wc16scasecmp(pDomain->pwszName, pwszBuiltinName));
            if (bIsBuiltin != bBuiltin) continue;

            pSamrDomain->pwszName = pDomain->pwszName;
            pSamrDomain->pSid     = pDomain->pSid;
            pSamrDomain->bLocal   = pDomain->bLocal;
            pSamrDomain->hDomain  = pDomain->hDomain;

            goto cleanup;
        }
    }

    memset(pSamrDomain, 0, sizeof(*pSamrDomain));
    status = STATUS_NO_SUCH_DOMAIN;

cleanup:
    if (pwszBuiltinName) {
        LW_SAFE_FREE_MEMORY(pwszBuiltinName);
    }

    return status;

error:
    memset(pSamrDomain, 0, sizeof(*pSamrDomain));
    goto cleanup;
}


NTSTATUS
LsaSrvLookupDomainsByAccountName(
    PPOLICY_CONTEXT pPolCtx,
    UnicodeStringEx *pNames,
    DWORD dwNumNames,
    handle_t hSamrBinding,
    PolicyHandle *phConn,
    PACCOUNT_NAMES *ppDomainAccounts,
    PACCOUNT_NAMES *ppLocalAccounts,
    PACCOUNT_NAMES *ppBuiltinAccounts,
    PACCOUNT_NAMES *ppOtherAccounts
    )
{
    const DWORD dwSamrDomainAccess = DOMAIN_ACCESS_LOOKUP_INFO_1 |
                                     DOMAIN_ACCESS_LOOKUP_ALIAS;

    NTSTATUS status = STATUS_SUCCESS;
    PACCOUNT_NAMES pDomainAccounts = NULL;
    PACCOUNT_NAMES pLocalAccounts = NULL;
    PACCOUNT_NAMES pBuiltinAccounts = NULL;
    PACCOUNT_NAMES pOtherAccounts = NULL;
    DWORD dwDomainNamesNum = 0;
    DWORD dwLocalNamesNum = 0;
    DWORD dwBuiltinNamesNum = 0;
    DWORD dwOtherNamesNum = 0;
    DWORD i = 0;
    PWSTR pwszName = NULL;
    PWSTR pwszDomainName = NULL;
    PWSTR pwszAcctName = NULL;
    DWORD dwResume = 0;
    DWORD dwMaxSize = -1;
    DWORD dwCount = 0;
    WCHAR wszBuiltin[] = LSA_BUILTIN_DOMAIN_NAME;
    PWSTR *pwszLocalDomainNames = NULL;
    SAMR_DOMAIN SamrDomain;
    PolicyHandle hDomain;
    PSID pDomainSid = NULL;

    status = LsaSrvAllocateMemory(
                            (void**)&pDomainAccounts,
                            sizeof(*pDomainAccounts));
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pDomainAccounts->ppwszNames,
                            sizeof(*pDomainAccounts->ppwszNames) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pDomainAccounts->pdwIndices,
                            sizeof(*pDomainAccounts->pdwIndices) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(pDomainAccounts->pdwIndices, -1, sizeof(DWORD) * dwNumNames);

    status = LsaSrvAllocateMemory(
                            (void**)&pLocalAccounts,
                            sizeof(*pLocalAccounts));
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pLocalAccounts->ppwszNames,
                            sizeof(*pLocalAccounts->ppwszNames) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pLocalAccounts->pdwIndices,
                            sizeof(*pLocalAccounts->pdwIndices) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(pLocalAccounts->pdwIndices, -1, sizeof(DWORD) * dwNumNames);

    status = LsaSrvAllocateMemory(
                            (void**)&pBuiltinAccounts,
                            sizeof(*pBuiltinAccounts));
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pBuiltinAccounts->ppwszNames,
                            sizeof(*pBuiltinAccounts->ppwszNames) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pBuiltinAccounts->pdwIndices,
                            sizeof(*pBuiltinAccounts->pdwIndices) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(pBuiltinAccounts->pdwIndices, -1, sizeof(DWORD) * dwNumNames);

    status = LsaSrvAllocateMemory(
                            (void**)&pOtherAccounts,
                            sizeof(*pOtherAccounts));
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pOtherAccounts->ppwszNames,
                            sizeof(*pOtherAccounts->ppwszNames) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    status = LsaSrvAllocateMemory(
                            (void**)&pOtherAccounts->pdwIndices,
                            sizeof(*pOtherAccounts->pdwIndices) * dwNumNames);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(pOtherAccounts->pdwIndices, -1, sizeof(DWORD) * dwNumNames);


    /* Get local and builtin domains info */
    status = SamrEnumDomains(hSamrBinding,
                             phConn,
                             &dwResume,
                             dwMaxSize,
                             &pwszLocalDomainNames,
                             &dwCount);
    BAIL_ON_NTSTATUS_ERROR(status);

    for (i = 0; i < dwCount; i++)
    {
        pwszDomainName = pwszLocalDomainNames[i];
        memset(&SamrDomain, 0, sizeof(SamrDomain));

        status = SamrLookupDomain(hSamrBinding,
                                  phConn,
                                  pwszDomainName,
                                  &pDomainSid);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = SamrOpenDomain(hSamrBinding,
                                phConn,
                                dwSamrDomainAccess,
                                pDomainSid,
                                &hDomain);
        BAIL_ON_NTSTATUS_ERROR(status);

        SamrDomain.pwszName = pwszDomainName;
        SamrDomain.pSid     = pDomainSid;
        SamrDomain.bLocal   = TRUE;
        SamrDomain.hDomain  = hDomain;

        status = LsaSrvSetSamrDomain(pPolCtx,
                                     &SamrDomain);
        BAIL_ON_NTSTATUS_ERROR(status);

        SamrFreeMemory(pDomainSid);
    }


    for (i = 0; i < dwNumNames; i++) {
        UnicodeStringEx *name = &(pNames[i]);

        status = LsaSrvGetFromUnicodeStringEx(&pwszName, name);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = LsaSrvParseAccountName(pwszName,
                                        &pwszDomainName,
                                        &pwszAcctName);
        BAIL_ON_NTSTATUS_ERROR(status);

        /* If account name wasn't prepended with domain name
           we assume it's an account in local domain */
        if (!pwszDomainName) {
            status = LsaSrvDuplicateWC16String(
                         &(pOtherAccounts->ppwszNames[dwOtherNamesNum]),
                         pwszAcctName);
            BAIL_ON_NTSTATUS_ERROR(status);

            pOtherAccounts->pdwIndices[dwOtherNamesNum] = i;
            dwOtherNamesNum++;

            continue;
        }

        memset(&SamrDomain, 0, sizeof(SamrDomain));

        status = LsaSrvGetSamrDomain(pPolCtx,
                                     pwszDomainName,
                                     &SamrDomain);
        if (status == STATUS_NO_SUCH_DOMAIN) {

            SamrDomain.pwszName = pwszDomainName;
            SamrDomain.pSid     = NULL;
            SamrDomain.bLocal   = FALSE;

            status = LsaSrvSetSamrDomain(pPolCtx,
                                         &SamrDomain);
            BAIL_ON_NTSTATUS_ERROR(status);

        } else if (status != STATUS_SUCCESS) {
            BAIL_ON_NTSTATUS_ERROR(status);
        }

        if (SamrDomain.bLocal) {
            if (!wc16scasecmp(SamrDomain.pwszName, wszBuiltin)) {
                status = LsaSrvDuplicateWC16String(
                             &(pBuiltinAccounts->ppwszNames[dwBuiltinNamesNum]),
                             pwszAcctName);
                BAIL_ON_NTSTATUS_ERROR(status);

                pBuiltinAccounts->pdwIndices[dwBuiltinNamesNum] = i;
                dwBuiltinNamesNum++;

            } else {
                status = LsaSrvDuplicateWC16String(
                             &(pLocalAccounts->ppwszNames[dwLocalNamesNum]),
                             pwszAcctName);
                BAIL_ON_NTSTATUS_ERROR(status);

                pLocalAccounts->pdwIndices[dwLocalNamesNum] = i;
                dwLocalNamesNum++;
            }

        } else {
            status = LsaSrvDuplicateWC16String(
                             &(pDomainAccounts->ppwszNames[dwDomainNamesNum]),
                             pwszName);
            BAIL_ON_NTSTATUS_ERROR(status);

            pDomainAccounts->pdwIndices[dwDomainNamesNum] = i;
            dwDomainNamesNum++;
        }


        if (pwszName) {
            LsaSrvFreeMemory(pwszName);
            pwszName = NULL;
        }

        if (pwszDomainName) {
            LsaSrvFreeMemory(pwszDomainName);
            pwszDomainName = NULL;
        }

        if (pwszAcctName) {
            LsaSrvFreeMemory(pwszAcctName);
            pwszAcctName = NULL;
        }
    }

    pDomainAccounts->dwCount  = dwDomainNamesNum;
    pLocalAccounts->dwCount   = dwLocalNamesNum;
    pBuiltinAccounts->dwCount = dwBuiltinNamesNum;
    pOtherAccounts->dwCount   = dwOtherNamesNum;

    *ppDomainAccounts   = pDomainAccounts;
    *ppLocalAccounts    = pLocalAccounts;
    *ppBuiltinAccounts  = pBuiltinAccounts;
    *ppOtherAccounts    = pOtherAccounts;

cleanup:
    if (pwszLocalDomainNames) {
        SamrFreeMemory(pwszLocalDomainNames);
    }

    return status;

error:

    *ppDomainAccounts   = NULL;
    *ppLocalAccounts    = NULL;
    *ppBuiltinAccounts  = NULL;
    *ppOtherAccounts    = NULL;
    goto cleanup;
}


void
LsaSrvFreeAccountNames(
    PACCOUNT_NAMES pAccountNames
    )
{
    DWORD iName = 0;

    for (iName = 0; iName < pAccountNames->dwCount; iName++)
    {
        LsaSrvFreeMemory(pAccountNames->ppwszNames[iName]);
    }

    LsaSrvFreeMemory(pAccountNames->ppwszNames);
    LsaSrvFreeMemory(pAccountNames);
}


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
