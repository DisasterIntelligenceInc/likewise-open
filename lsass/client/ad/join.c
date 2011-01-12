/*
 * Copyright Likewise Software
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
 *        join.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Domain membership management API
 *
 * Authors: Brian Koropoff (bkoropoff@likewise.com)
 */
#include "adclient.h"

DWORD
LsaAdJoinDomain(
    HANDLE hLsaConnection,
    PCSTR pszHostname,
    PCSTR pszHostDnsDomain,
    PCSTR pszDomain,
    PCSTR pszOU,
    PCSTR pszUsername,
    PCSTR pszPassword,
    PCSTR pszOSName,
    PCSTR pszOSVersion,
    PCSTR pszOSServicePack,
    LSA_NET_JOIN_FLAGS dwFlags
    )
{
    DWORD dwError = 0;
    LWMsgDataContext* pDataContext = NULL;
    LSA_AD_IPC_JOIN_DOMAIN_REQ request;
    PVOID pBlob = NULL;
    size_t blobSize = 0;

    request.pszHostname = pszHostname;
    request.pszHostDnsDomain = pszHostDnsDomain;
    request.pszDomain = pszDomain;
    request.pszOU = pszOU;
    request.pszUsername = pszUsername;
    request.pszPassword = pszPassword;
    request.pszOSName = pszOSName;
    request.pszOSVersion = pszOSVersion;
    request.pszOSServicePack = pszOSServicePack;
    request.dwFlags = dwFlags;

    dwError = MAP_LWMSG_ERROR(lwmsg_data_context_new(NULL, &pDataContext));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_marshal_flat_alloc(
                                  pDataContext,
                                  LsaAdIPCGetJoinDomainReqSpec(),
                                  &request,
                                  &pBlob,
                                  &blobSize));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaProviderIoControl(
        hLsaConnection,
        LSA_PROVIDER_TAG_AD,
        LSA_AD_IO_JOINDOMAIN,
        (DWORD) blobSize,
        pBlob,
        NULL,
        NULL);
    BAIL_ON_LSA_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_MEMORY(pBlob);

    if (pDataContext)
    {
        lwmsg_data_context_delete(pDataContext);
    }

    return dwError;

error:

    goto cleanup;
}

DWORD
LsaAdLeaveDomain(
    HANDLE hLsaConnection,
    PCSTR pszUsername,
    PCSTR pszPassword
    )
{
    return LsaAdLeaveDomain2(
               hLsaConnection,
               pszUsername,
               pszPassword,
               NULL,
               0);
}

DWORD
LsaAdLeaveDomain2(
    HANDLE hLsaConnection,
    PCSTR pszUsername,
    PCSTR pszPassword,
    PCSTR pszDomain,
    LSA_NET_JOIN_FLAGS dwFlags
    )
{
    DWORD dwError = 0;
    LWMsgDataContext* pDataContext = NULL;
    LSA_AD_IPC_LEAVE_DOMAIN_REQ request;
    PVOID pBlob = NULL;
    size_t blobSize = 0;

    request.pszUsername = pszUsername;
    request.pszPassword = pszPassword;
    request.pszDomain = pszDomain;
    request.dwFlags = dwFlags;

    dwError = MAP_LWMSG_ERROR(lwmsg_data_context_new(NULL, &pDataContext));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_marshal_flat_alloc(
                                  pDataContext,
                                  LsaAdIPCGetLeaveDomainReqSpec(),
                                  &request,
                                  &pBlob,
                                  &blobSize));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaProviderIoControl(
        hLsaConnection,
        LSA_PROVIDER_TAG_AD,
        LSA_AD_IO_LEAVEDOMAIN,
        (DWORD) blobSize,
        pBlob,
        NULL,
        NULL);
    BAIL_ON_LSA_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_MEMORY(pBlob);

    if (pDataContext)
    {
        lwmsg_data_context_delete(pDataContext);
    }

    return dwError;

error:

    goto cleanup;
}

DWORD
LsaAdSetDefaultDomain(
    IN HANDLE hLsaConnection,
    IN PCSTR pszDomain
    )
{
    DWORD dwError = 0;

    if (geteuid() != 0)
    {
        dwError = LW_ERROR_ACCESS_DENIED;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LsaProviderIoControl(
                  hLsaConnection,
                  LSA_PROVIDER_TAG_AD,
                  LSA_AD_IO_SETDEFAULTDOMAIN,
                  strlen(pszDomain) + 1,
                  (PVOID)pszDomain,
                  NULL,
                  NULL);
    BAIL_ON_LSA_ERROR(dwError);

error:

    return dwError;
}

DWORD
LsaAdGetJoinedDomains(
    IN HANDLE hLsaConnection,
    OUT PDWORD pdwNumDomainsFound,
    OUT PSTR** pppszJoinedDomains
    )
{
    DWORD dwError = 0;
    DWORD dwOutputBufferSize = 0;
    PVOID pOutputBuffer = NULL;
    LWMsgContext* context = NULL;
    LWMsgDataContext* pDataContext = NULL;
    PLSA_AD_IPC_GET_JOINED_DOMAINS_RESP response = NULL;

    dwError = LsaProviderIoControl(
                  hLsaConnection,
                  LSA_PROVIDER_TAG_AD,
                  LSA_AD_IO_GETJOINEDDOMAINS,
                  0,
                  NULL,
                  &dwOutputBufferSize,
                  &pOutputBuffer);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_context_new(NULL, &context));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_context_new(context, &pDataContext));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_unmarshal_flat(
                              pDataContext,
                              LsaAdIPCGetJoinedDomainsRespSpec(),
                              pOutputBuffer,
                              dwOutputBufferSize,
                              (PVOID*)&response));
    BAIL_ON_LSA_ERROR(dwError);

    *pdwNumDomainsFound = response->dwObjectsCount;
    *pppszJoinedDomains = response->ppszDomains;
    response->ppszDomains = NULL;

cleanup:

    if ( response )
    {
        lwmsg_data_free_graph(
            pDataContext,
            LsaAdIPCGetJoinedDomainsRespSpec(),
            response);
    }

    if (pDataContext)
    {
        lwmsg_data_context_delete(pDataContext);
    }

    if ( context )
    {
        lwmsg_context_delete(context);
    }

    if ( pOutputBuffer )
    {
        LwFreeMemory(pOutputBuffer);
    }

    return dwError;

error:

    *pdwNumDomainsFound = 0;
    *pppszJoinedDomains = NULL;

    goto cleanup;
}

static
inline
VOID
LsaAdpFreeMachineAccountInfoContents(
    IN OUT PLSA_MACHINE_ACCOUNT_INFO_A pAccountInfo
    )
{
    LW_SAFE_FREE_STRING(pAccountInfo->DnsDomainName);
    LW_SAFE_FREE_STRING(pAccountInfo->NetbiosDomainName);
    LW_SAFE_FREE_STRING(pAccountInfo->DomainSid);
    LW_SAFE_FREE_STRING(pAccountInfo->SamAccountName);
    LW_SAFE_FREE_STRING(pAccountInfo->Fqdn);
}

LW_DWORD
LsaAdGetMachineAccountInfo(
    LW_IN LW_HANDLE hLsaConnection,
    LW_IN LW_OPTIONAL LW_PCSTR pszDnsDomainName,
    LW_OUT PLSA_MACHINE_ACCOUNT_INFO_A* ppAccountInfo
    )
{
    DWORD dwError = 0;
    size_t inputBufferSize = 0;
    PVOID pInputBuffer = NULL;
    DWORD dwOutputBufferSize = 0;
    PVOID pOutputBuffer = NULL;
    LWMsgContext* pContext = NULL;
    LWMsgDataContext* pDataContext = NULL;
    PLSA_MACHINE_ACCOUNT_INFO_A pAccountInfo = NULL;

    dwError = MAP_LWMSG_ERROR(lwmsg_context_new(NULL, &pContext));
    BAIL_ON_LSA_ERROR(dwError);

    LsaAdIPCSetMemoryFunctions(pContext);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_context_new(pContext, &pDataContext));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_marshal_flat_alloc(
                                  pDataContext,
                                  LsaAdIPCGetStringSpec(),
                                  (PVOID) pszDnsDomainName,
                                  &pInputBuffer,
                                  &inputBufferSize));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaProviderIoControl(
                  hLsaConnection,
                  LSA_PROVIDER_TAG_AD,
                  LSA_AD_IO_GET_MACHINE_ACCOUNT,
                  inputBufferSize,
                  pInputBuffer,
                  &dwOutputBufferSize,
                  &pOutputBuffer);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_unmarshal_flat(
                              pDataContext,
                              LsaAdIPCGetMachineAccountInfoSpec(),
                              pOutputBuffer,
                              dwOutputBufferSize,
                              OUT_PPVOID(&pAccountInfo)));
    BAIL_ON_LSA_ERROR(dwError);

error:
    if (dwError)
    {
        if (pAccountInfo)
        {
            LsaAdFreeMachineAccountInfo(pAccountInfo);
            pAccountInfo = NULL;
        }
    }

    if (pOutputBuffer)
    {
        LwFreeMemory(pOutputBuffer);
    }

    if (pInputBuffer)
    {
        LwFreeMemory(pInputBuffer);
    }

    if (pDataContext)
    {
        lwmsg_data_context_delete(pDataContext);
    }

    if (pContext)
    {
        lwmsg_context_delete(pContext);
    }

    *ppAccountInfo = pAccountInfo;

    return dwError;
}

LW_VOID
LsaAdFreeMachineAccountInfo(
    IN PLSA_MACHINE_ACCOUNT_INFO_A pAccountInfo
    )
{
    if (pAccountInfo)
    {
        LsaAdpFreeMachineAccountInfoContents(pAccountInfo);
        LwFreeMemory(pAccountInfo);
    }
}

LW_DWORD
LsaAdGetMachinePasswordInfo(
    LW_IN LW_HANDLE hLsaConnection,
    LW_IN LW_PCSTR pszDnsDomainName,
    LW_OUT PLSA_MACHINE_PASSWORD_INFO_A* ppPasswordInfo
    )
{
    DWORD dwError = 0;
    size_t inputBufferSize = 0;
    PVOID pInputBuffer = NULL;
    DWORD dwOutputBufferSize = 0;
    PVOID pOutputBuffer = NULL;
    LWMsgContext* pContext = NULL;
    LWMsgDataContext* pDataContext = NULL;
    PLSA_MACHINE_PASSWORD_INFO_A pPasswordInfo = NULL;

    dwError = MAP_LWMSG_ERROR(lwmsg_context_new(NULL, &pContext));
    BAIL_ON_LSA_ERROR(dwError);

    LsaAdIPCSetMemoryFunctions(pContext);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_context_new(pContext, &pDataContext));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_marshal_flat_alloc(
                                  pDataContext,
                                  LsaAdIPCGetStringSpec(),
                                  (PVOID) pszDnsDomainName,
                                  &pInputBuffer,
                                  &inputBufferSize));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaProviderIoControl(
                  hLsaConnection,
                  LSA_PROVIDER_TAG_AD,
                  LSA_AD_IO_GET_MACHINE_PASSWORD,
                  inputBufferSize,
                  pInputBuffer,
                  &dwOutputBufferSize,
                  &pOutputBuffer);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_unmarshal_flat(
                              pDataContext,
                              LsaAdIPCGetMachinePasswordInfoSpec(),
                              pOutputBuffer,
                              dwOutputBufferSize,
                              OUT_PPVOID(&pPasswordInfo)));
    BAIL_ON_LSA_ERROR(dwError);

error:
    if (dwError)
    {
        if (pPasswordInfo)
        {
            LsaAdFreeMachinePasswordInfo(pPasswordInfo);
            pPasswordInfo = NULL;
        }
    }

    if (pOutputBuffer)
    {
        LwFreeMemory(pOutputBuffer);
    }

    if (pInputBuffer)
    {
        LwFreeMemory(pInputBuffer);
    }

    if (pDataContext)
    {
        lwmsg_data_context_delete(pDataContext);
    }

    if (pContext)
    {
        lwmsg_context_delete(pContext);
    }

    *ppPasswordInfo = pPasswordInfo;

    return dwError;
}

LW_VOID
LsaAdFreeMachinePasswordInfo(
    LW_IN PLSA_MACHINE_PASSWORD_INFO_A pPasswordInfo
    )
{
    if (pPasswordInfo)
    {
        LsaAdpFreeMachineAccountInfoContents(&pPasswordInfo->Account);
        LW_SECURE_FREE_STRING(pPasswordInfo->Password);
        LwFreeMemory(pPasswordInfo);
    }
}

LW_DWORD
LsaAdGetComputerDn(
    LW_IN LW_HANDLE hLsaConnection,
    LW_IN LW_OPTIONAL LW_PCSTR pszDnsDomainName,
    LW_OUT LW_PSTR* ppszComputerDn
    )
{
    DWORD dwError = 0;
    size_t inputBufferSize = 0;
    PVOID pInputBuffer = NULL;
    DWORD dwOutputBufferSize = 0;
    PVOID pOutputBuffer = NULL;
    LWMsgContext* pContext = NULL;
    LWMsgDataContext* pDataContext = NULL;
    PSTR pszComputerDn = NULL;

    dwError = MAP_LWMSG_ERROR(lwmsg_context_new(NULL, &pContext));
    BAIL_ON_LSA_ERROR(dwError);

    LsaAdIPCSetMemoryFunctions(pContext);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_context_new(pContext, &pDataContext));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_marshal_flat_alloc(
                                    pDataContext,
                                    LsaAdIPCGetStringSpec(),
                                    (PVOID) pszDnsDomainName,
                                    &pInputBuffer,
                                    &inputBufferSize));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaProviderIoControl(
                    hLsaConnection,
                    LSA_PROVIDER_TAG_AD,
                    LSA_AD_IO_GET_COMPUTER_DN,
                    inputBufferSize,
                    pInputBuffer,
                    &dwOutputBufferSize,
                    &pOutputBuffer);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = MAP_LWMSG_ERROR(lwmsg_data_unmarshal_flat(
                                    pDataContext,
                                    LsaAdIPCGetStringSpec(),
                                    pOutputBuffer,
                                    dwOutputBufferSize,
                                    OUT_PPVOID(&pszComputerDn)));
    BAIL_ON_LSA_ERROR(dwError);

error:
    if (dwError)
    {
        LW_SAFE_FREE_STRING(pszComputerDn);
    }

    if (pOutputBuffer)
    {
        LwFreeMemory(pOutputBuffer);
    }

    if (pInputBuffer)
    {
        LwFreeMemory(pInputBuffer);
    }

    if (pDataContext)
    {
        lwmsg_data_context_delete(pDataContext);
    }

    if (pContext)
    {
        lwmsg_context_delete(pContext);
    }

    *ppszComputerDn = pszComputerDn;

    return dwError;
}
