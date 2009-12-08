/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

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
 *        lsassdb.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Local Authentication Provider
 *
 *        Directory User Management API
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 */
#include "includes.h"

static
DWORD
LocalDirBeginEnumUsers_0(
    HANDLE  hProvider,
    PHANDLE phResume
    );

static
DWORD
LocalDirBeginEnumUsers_1(
    HANDLE  hProvider,
    PHANDLE phResume
    );

static
DWORD
LocalDirBeginEnumUsers_2(
    HANDLE  hProvider,
    PHANDLE phResume
    );

static
DWORD
LocalDirEnumUsers_0(
    HANDLE                     hProvider,
    PLOCAL_PROVIDER_ENUM_STATE pEnumState,
    DWORD                      dwNumMaxUsers,
    PDWORD                     pdwNumUsersFound,
    PVOID**                    pppUserInfoList
    );

static
DWORD
LocalDirEnumUsers_1(
    HANDLE                     hProvider,
    PLOCAL_PROVIDER_ENUM_STATE pEnumState,
    DWORD                      dwNumMaxUsers,
    PDWORD                     pdwNumUsersFound,
    PVOID**                    pppUserInfoList
    );

static
DWORD
LocalDirEnumUsers_2(
    HANDLE                     hProvider,
    PLOCAL_PROVIDER_ENUM_STATE pEnumState,
    DWORD                      dwNumMaxUsers,
    PDWORD                     pdwNumUsersFound,
    PVOID**                    pppUserInfoList
    );

static
DWORD
LocalDirValidateUID(
    uid_t uid
    );

static
DWORD
LocalDirGetBuiltinAdministratorsGID(
    HANDLE hProvider,
    gid_t* pGID
    );

DWORD
LocalDirFindUserByName(
    HANDLE hProvider,
    PCSTR  pszDomainName,
    PCSTR  pszUserName,
    DWORD  dwUserInfoLevel,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;

    switch(dwUserInfoLevel)
    {
        case 0:

            dwError = LocalDirFindUserByName_0(
                            hProvider,
                            pszDomainName,
                            pszUserName,
                            ppwszUserDN,
                            ppUserInfo
                            );
            break;

        case 1:

            dwError = LocalDirFindUserByName_1(
                            hProvider,
                            pszDomainName,
                            pszUserName,
                            ppwszUserDN,
                            ppUserInfo
                            );
            break;

        case 2:

            dwError = LocalDirFindUserByName_2(
                            hProvider,
                            pszDomainName,
                            pszUserName,
                            ppwszUserDN,
                            ppUserInfo
                            );
            break;

        default:

            dwError = LW_ERROR_UNSUPPORTED_USER_LEVEL;

            break;
    }

    return dwError;
}

DWORD
LocalDirFindUserByName_0(
    HANDLE hProvider,
    PCSTR  pszDomainName,
    PCSTR  pszUserName,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameDN[]             = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameDN[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwNumEntries = 0;
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_SAM_ACCOUNT_NAME " = \"%s\"" \
                    " AND " LOCAL_DB_DIR_ATTR_DOMAIN   " = \"%s\"" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PWSTR pwszUserDN = NULL;
    PLSA_USER_INFO_0 pUserInfo = NULL;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    pszUserName,
                    pszDomainName,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalEntryToUserInfo_0(
                    pEntry,
                    ppwszUserDN ? &pwszUserDN : NULL,
                    &pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (ppwszUserDN)
    {
        *ppwszUserDN = pwszUserDN;
    }
    *ppUserInfo = pUserInfo;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    return dwError;

error:

    if (ppwszUserDN)
    {
        *ppwszUserDN = NULL;
    }
    *ppUserInfo = pUserInfo;

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    if (pUserInfo)
    {
        LsaFreeUserInfo(0, pUserInfo);
    }

    goto cleanup;
}

DWORD
LocalDirFindUserByName_1(
    HANDLE hProvider,
    PCSTR  pszDomainName,
    PCSTR  pszUserName,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[] = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[] = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]  = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameGecos[]     = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]     = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]   = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameUPN[]       = LOCAL_DIR_ATTR_USER_PRINCIPAL_NAME;
    wchar16_t wszAttrNameObjectSID[] = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameDN[]        = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    wchar16_t wszAttrNameNetBIOSDomain[] = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameUPN[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameDN[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwNumEntries = 0;
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_SAM_ACCOUNT_NAME " = \"%s\"" \
                    " AND " LOCAL_DB_DIR_ATTR_DOMAIN   " = \"%s\"" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR  pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PWSTR pwszUserDN = NULL;
    DWORD dwInfoLevel = 1;
    PLSA_USER_INFO_1 pUserInfo = NULL;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    pszUserName,
                    pszDomainName,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalEntryToUserInfo_1(
                    pEntry,
                    pszDomainName,
                    ppwszUserDN ? &pwszUserDN : NULL,
                    &pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (ppwszUserDN)
    {
        *ppwszUserDN = pwszUserDN;
    }
    *ppUserInfo = pUserInfo;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    return dwError;

error:

    if (ppwszUserDN)
    {
        *ppwszUserDN = NULL;
    }
    *ppUserInfo = pUserInfo;

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    if (pUserInfo)
    {
        LsaFreeUserInfo(dwInfoLevel, pUserInfo);
    }

    goto cleanup;
}

DWORD
LocalDirFindUserByName_2(
    HANDLE hProvider,
    PCSTR  pszDomainName,
    PCSTR  pszUserName,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameNTHash[]         = LOCAL_DIR_ATTR_NT_HASH;
    wchar16_t wszAttrNameLMHash[]         = LOCAL_DIR_ATTR_LM_HASH;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameUPN[]            = LOCAL_DIR_ATTR_USER_PRINCIPAL_NAME;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameUserInfoFlags[]  = LOCAL_DIR_ATTR_ACCOUNT_FLAGS;
    wchar16_t wszAttrNameAccountExpiry[]  = LOCAL_DIR_ATTR_ACCOUNT_EXPIRY;
    wchar16_t wszAttrNamePasswdLastSet[]  = LOCAL_DIR_ATTR_PASSWORD_LAST_SET;
    wchar16_t wszAttrNameDN[]             = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameNTHash[0],
        &wszAttrNameLMHash[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameUPN[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameUserInfoFlags[0],
        &wszAttrNameAccountExpiry[0],
        &wszAttrNamePasswdLastSet[0],
        &wszAttrNameDN[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwNumEntries = 0;
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_SAM_ACCOUNT_NAME " = \"%s\"" \
                    " AND " LOCAL_DB_DIR_ATTR_DOMAIN   " = \"%s\"" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR   pszFilter = NULL;
    PWSTR  pwszFilter = NULL;
    PWSTR  pwszUserDN = NULL;
    DWORD  dwInfoLevel = 2;
    PLSA_USER_INFO_2 pUserInfo = NULL;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    pszUserName,
                    pszDomainName,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalEntryToUserInfo_2(
                    pEntry,
                    pszDomainName,
                    ppwszUserDN ? &pwszUserDN : NULL,
                    &pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (ppwszUserDN)
    {
        *ppwszUserDN = pwszUserDN;
    }
    *ppUserInfo = pUserInfo;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    return dwError;

error:

    if (ppwszUserDN)
    {
        *ppwszUserDN = NULL;
    }
    *ppUserInfo = pUserInfo;

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    if (pUserInfo)
    {
        LsaFreeUserInfo(dwInfoLevel, pUserInfo);
    }

    goto cleanup;
}

DWORD
LocalDirFindUserById(
    HANDLE hProvider,
    uid_t  uid,
    DWORD  dwInfoLevel,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;

    if (uid == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
        BAIL_ON_LSA_ERROR(dwError);
    }

    switch(dwInfoLevel)
    {
        case 0:

            dwError = LocalDirFindUserById_0(
                            hProvider,
                            uid,
                            ppwszUserDN,
                            ppUserInfo);

            break;

        case 1:

            dwError = LocalDirFindUserById_1(
                            hProvider,
                            uid,
                            ppwszUserDN,
                            ppUserInfo);

            break;

        case 2:

            dwError = LocalDirFindUserById_2(
                            hProvider,
                            uid,
                            ppwszUserDN,
                            ppUserInfo);

            break;

        default:

            dwError = LW_ERROR_UNSUPPORTED_USER_LEVEL;

            break;
    }

error:

    return dwError;
}

DWORD
LocalDirFindUserById_0(
    HANDLE hProvider,
    uid_t  uid,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameDN[]             = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameDN[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwNumEntries = 0;
    PCSTR pszFilterTemplate = LOCAL_DB_DIR_ATTR_UID " = %u" \
                              " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PLSA_USER_INFO_0 pUserInfo = NULL;
    PWSTR pwszUserDN = NULL;

    // Should we include the domain also?
    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    uid,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalEntryToUserInfo_0(
                    pEntry,
                    ppwszUserDN ? &pwszUserDN : NULL,
                    &pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (ppwszUserDN)
    {
        *ppwszUserDN = pwszUserDN;
    }
    *ppUserInfo = pUserInfo;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    return dwError;

error:

    if (ppwszUserDN)
    {
        *ppwszUserDN = NULL;
    }
    *ppUserInfo = NULL;

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    if (pUserInfo)
    {
        LsaFreeUserInfo(0, pUserInfo);
    }

    goto cleanup;
}

DWORD
LocalDirFindUserById_1(
    HANDLE hProvider,
    uid_t  uid,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameUPN[]            = LOCAL_DIR_ATTR_USER_PRINCIPAL_NAME;
    wchar16_t wszAttrNameDomain[]         = LOCAL_DIR_ATTR_DOMAIN;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameDN[]             = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameUPN[0],
        &wszAttrNameDomain[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameDN[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwNumEntries = 0;
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_UID " = %u" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR  pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PSTR  pszDomain = NULL;
    DWORD dwInfoLevel = 1;
    PLSA_USER_INFO_1 pUserInfo = NULL;
    PWSTR pwszUserDN = NULL;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    uid,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalAttrToANSIFromUnicodeString(
                    pEntry,
                    &wszAttrNameDomain[0],
                    &pszDomain);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LocalMarshalEntryToUserInfo_1(
                    pEntry,
                    pszDomain,
                    ppwszUserDN ? &pwszUserDN : NULL,
                    &pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (ppwszUserDN)
    {
        *ppwszUserDN = pwszUserDN;
    }
    *ppUserInfo = pUserInfo;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);
    LW_SAFE_FREE_STRING(pszDomain);

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    return dwError;

error:

    if (ppwszUserDN)
    {
        *ppwszUserDN = NULL;
    }
    *ppUserInfo = NULL;

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    if (pUserInfo)
    {
        LsaFreeUserInfo(dwInfoLevel, pUserInfo);
    }

    goto cleanup;
}

DWORD
LocalDirFindUserById_2(
    HANDLE hProvider,
    uid_t  uid,
    PWSTR* ppwszUserDN,
    PVOID* ppUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameNTHash[]         = LOCAL_DIR_ATTR_NT_HASH;
    wchar16_t wszAttrNameLMHash[]         = LOCAL_DIR_ATTR_LM_HASH;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameUPN[]            = LOCAL_DIR_ATTR_USER_PRINCIPAL_NAME;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameUserInfoFlags[]  = LOCAL_DIR_ATTR_ACCOUNT_FLAGS;
    wchar16_t wszAttrNameAccountExpiry[]  = LOCAL_DIR_ATTR_ACCOUNT_EXPIRY;
    wchar16_t wszAttrNamePasswdLastSet[]  = LOCAL_DIR_ATTR_PASSWORD_LAST_SET;
    wchar16_t wszAttrNameDomain[]         = LOCAL_DIR_ATTR_DOMAIN;
    wchar16_t wszAttrNameDN[]             = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameNTHash[0],
        &wszAttrNameLMHash[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameUPN[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameUserInfoFlags[0],
        &wszAttrNameAccountExpiry[0],
        &wszAttrNamePasswdLastSet[0],
        &wszAttrNameDomain[0],
        &wszAttrNameDN[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    DWORD dwNumAttrs = (sizeof(wszAttrs)/sizeof(wszAttrs[0])) - 1;
    PDIRECTORY_ENTRY pEntries = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    DWORD dwNumEntries = 0;
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_UID " = %u" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR  pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PSTR  pszDomain = NULL;
    DWORD dwInfoLevel = 2;
    PLSA_USER_INFO_2 pUserInfo = NULL;
    PWSTR pwszUserDN = NULL;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    uid,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalAttrToANSIFromUnicodeString(
                    pEntry,
                    &wszAttrNameDomain[0],
                    &pszDomain);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LocalMarshalEntryToUserInfo_2(
                    pEntry,
                    pszDomain,
                    ppwszUserDN ? &pwszUserDN : NULL,
                    &pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (ppwszUserDN)
    {
        *ppwszUserDN = pwszUserDN;
    }
    *ppUserInfo = pUserInfo;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);
    LW_SAFE_FREE_STRING(pszDomain);

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    return dwError;

error:

    if (ppwszUserDN)
    {
        *ppwszUserDN = NULL;
    }
    *ppUserInfo = NULL;

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    if (pUserInfo)
    {
        LsaFreeUserInfo(dwInfoLevel, pUserInfo);
    }

    goto cleanup;
}

DWORD
LocalDirGetUserInfoFlags(
    HANDLE hProvider,
    uid_t  uid,
    PDWORD pdwUserInfoFlags
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUserInfoFlags[]  = LOCAL_DIR_ATTR_ACCOUNT_FLAGS;
    PWSTR pwszAttrs[] =
    {
            &wszAttrNameUserInfoFlags[0],
            NULL
    };
    DWORD dwNumAttrs = (sizeof(pwszAttrs)/sizeof(pwszAttrs[0])) - 1;
    PCSTR pszFilterTemplate =
                        LOCAL_DB_DIR_ATTR_UID " = %u" \
                        " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR  pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PDIRECTORY_ENTRY pEntry = NULL;
    PDIRECTORY_ENTRY pEntries = NULL;
    DWORD dwNumEntries = 0;
    DWORD dwUserInfoFlags = 0;

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    uid,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    pwszAttrs,
                    FALSE,
                    &pEntries,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (dwNumEntries == 0)
    {
        dwError = LW_ERROR_NO_SUCH_USER;
    }
    else if (dwNumEntries != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
    }
    BAIL_ON_LSA_ERROR(dwError);

    pEntry = &pEntries[0];
    if (pEntry->ulNumAttributes != dwNumAttrs)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalMarshalAttrToInteger(
                    pEntry,
                    &wszAttrNameUserInfoFlags[0],
                    &dwUserInfoFlags);
    BAIL_ON_LSA_ERROR(dwError);

    *pdwUserInfoFlags = dwUserInfoFlags;

cleanup:

    if (pEntries)
    {
        DirectoryFreeEntries(pEntries, dwNumEntries);
    }

    LW_SAFE_FREE_MEMORY(pwszFilter);
    LW_SAFE_FREE_STRING(pszFilter);

    return dwError;

error:

    *pdwUserInfoFlags = 0;

    goto cleanup;
}

DWORD
LocalDirBeginEnumUsers(
    HANDLE  hProvider,
    DWORD   dwInfoLevel,
    PHANDLE phResume
    )
{
    DWORD dwError = 0;

    switch (dwInfoLevel)
    {
        case 0:

            dwError = LocalDirBeginEnumUsers_0(
                            hProvider,
                            phResume);

            break;

        case 1:

            dwError = LocalDirBeginEnumUsers_1(
                            hProvider,
                            phResume);

            break;

        case 2:

            dwError = LocalDirBeginEnumUsers_2(
                            hProvider,
                            phResume);

            break;

        default:

            dwError = LW_ERROR_UNSUPPORTED_USER_LEVEL;

            break;
    }

    return dwError;
}

static
DWORD
LocalDirBeginEnumUsers_0(
    HANDLE  hProvider,
    PHANDLE phResume
    )
{
    DWORD dwError = 0;
    DWORD dwInfoLevel = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_DOMAIN   " IN (\"%s\", \"%s\")" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PLOCAL_PROVIDER_ENUM_STATE pEnumState = NULL;

    dwError = LocalCreateUserState(
                        hProvider,
                        dwInfoLevel,
                        &pEnumState);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    gLPGlobals.pszLocalDomain,
                    gLPGlobals.pszBuiltinDomain,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEnumState->pEntries,
                    &pEnumState->dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    *phResume = (HANDLE)pEnumState;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    return dwError;

error:

    *phResume = (HANDLE)NULL;

    if (pEnumState)
    {
        LocalFreeEnumState(pEnumState);
    }

    goto cleanup;
}

static
DWORD
LocalDirBeginEnumUsers_1(
    HANDLE  hProvider,
    PHANDLE phResume
    )
{
    DWORD dwError = 0;
    DWORD dwInfoLevel = 1;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameUPN[]            = LOCAL_DIR_ATTR_USER_PRINCIPAL_NAME;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameUPN[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_DOMAIN   " IN (\"%s\", \"%s\")" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PLOCAL_PROVIDER_ENUM_STATE pEnumState = NULL;

    dwError = LocalCreateUserState(
                        hProvider,
                        dwInfoLevel,
                        &pEnumState);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    gLPGlobals.pszLocalDomain,
                    gLPGlobals.pszBuiltinDomain,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEnumState->pEntries,
                    &pEnumState->dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    *phResume = (HANDLE)pEnumState;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    return dwError;

error:

    *phResume = (HANDLE)NULL;

    if (pEnumState)
    {
        LocalFreeEnumState(pEnumState);
    }

    goto cleanup;
}

static
DWORD
LocalDirBeginEnumUsers_2(
    HANDLE  hProvider,
    PHANDLE phResume
    )
{
    DWORD dwError = 0;
    DWORD dwInfoLevel = 2;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    wchar16_t wszAttrNameUID[]            = LOCAL_DIR_ATTR_UID;
    wchar16_t wszAttrNameGID[]            = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    wchar16_t wszAttrNameSamAccountName[] = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    wchar16_t wszAttrNamePassword[]       = LOCAL_DIR_ATTR_PASSWORD;
    wchar16_t wszAttrNameNTHash[]         = LOCAL_DIR_ATTR_NT_HASH;
    wchar16_t wszAttrNameLMHash[]         = LOCAL_DIR_ATTR_LM_HASH;
    wchar16_t wszAttrNameGecos[]          = LOCAL_DIR_ATTR_GECOS;
    wchar16_t wszAttrNameShell[]          = LOCAL_DIR_ATTR_SHELL;
    wchar16_t wszAttrNameHomedir[]        = LOCAL_DIR_ATTR_HOME_DIR;
    wchar16_t wszAttrNameUPN[]            = LOCAL_DIR_ATTR_USER_PRINCIPAL_NAME;
    wchar16_t wszAttrNameObjectSID[]      = LOCAL_DIR_ATTR_OBJECT_SID;
    wchar16_t wszAttrNameUserInfoFlags[]  = LOCAL_DIR_ATTR_ACCOUNT_FLAGS;
    wchar16_t wszAttrNameAccountExpiry[]  = LOCAL_DIR_ATTR_ACCOUNT_EXPIRY;
    wchar16_t wszAttrNamePasswdLastSet[]  = LOCAL_DIR_ATTR_PASSWORD_LAST_SET;
    wchar16_t wszAttrNameNetBIOSDomain[]      = LOCAL_DIR_ATTR_NETBIOS_NAME;
    PWSTR wszAttrs[] =
    {
        &wszAttrNameUID[0],
        &wszAttrNameGID[0],
        &wszAttrNameSamAccountName[0],
        &wszAttrNamePassword[0],
        &wszAttrNameNTHash[0],
        &wszAttrNameLMHash[0],
        &wszAttrNameGecos[0],
        &wszAttrNameShell[0],
        &wszAttrNameHomedir[0],
        &wszAttrNameUPN[0],
        &wszAttrNameObjectSID[0],
        &wszAttrNameUserInfoFlags[0],
        &wszAttrNameAccountExpiry[0],
        &wszAttrNamePasswdLastSet[0],
        &wszAttrNameNetBIOSDomain[0],
        NULL
    };
    PCSTR pszFilterTemplate =
                    LOCAL_DB_DIR_ATTR_DOMAIN   " IN (\"%s\", \"%s\")" \
                    " AND " LOCAL_DB_DIR_ATTR_OBJECT_CLASS " = %d";
    PSTR pszFilter = NULL;
    PWSTR pwszFilter = NULL;
    PLOCAL_PROVIDER_ENUM_STATE pEnumState = NULL;

    dwError = LocalCreateUserState(
                        hProvider,
                        dwInfoLevel,
                        &pEnumState);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateStringPrintf(
                    &pszFilter,
                    pszFilterTemplate,
                    gLPGlobals.pszLocalDomain,
                    gLPGlobals.pszBuiltinDomain,
                    LOCAL_OBJECT_CLASS_USER);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaMbsToWc16s(
                    pszFilter,
                    &pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    NULL,
                    0,
                    pwszFilter,
                    wszAttrs,
                    FALSE,
                    &pEnumState->pEntries,
                    &pEnumState->dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    *phResume = (HANDLE)pEnumState;

cleanup:

    LW_SAFE_FREE_STRING(pszFilter);
    LW_SAFE_FREE_MEMORY(pwszFilter);

    return dwError;

error:

    *phResume = (HANDLE)NULL;

    if (pEnumState)
    {
        LocalFreeEnumState(pEnumState);
    }

    goto cleanup;
}

DWORD
LocalDirEnumUsers(
    HANDLE  hProvider,
    HANDLE  hResume,
    DWORD   nMaxUsers,
    PDWORD  pdwNumUsersFound,
    PVOID** pppUserInfoList
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_ENUM_STATE pEnumState = (PLOCAL_PROVIDER_ENUM_STATE)hResume;

    BAIL_ON_INVALID_POINTER(pEnumState);

    switch (pEnumState->dwInfoLevel)
    {
        case 0:

            dwError = LocalDirEnumUsers_0(
                                hProvider,
                                pEnumState,
                                nMaxUsers,
                                pdwNumUsersFound,
                                pppUserInfoList
                                );
            break;

        case 1:

            dwError = LocalDirEnumUsers_1(
                                hProvider,
                                pEnumState,
                                nMaxUsers,
                                pdwNumUsersFound,
                                pppUserInfoList
                                );
            break;

        case 2:

            dwError = LocalDirEnumUsers_2(
                                hProvider,
                                pEnumState,
                                nMaxUsers,
                                pdwNumUsersFound,
                                pppUserInfoList
                                );
            break;

        default:

            dwError = LW_ERROR_UNSUPPORTED_USER_LEVEL;

            break;
    }

error:

    return dwError;
}

static
DWORD
LocalDirEnumUsers_0(
    HANDLE                     hProvider,
    PLOCAL_PROVIDER_ENUM_STATE pEnumState,
    DWORD                      dwNumMaxUsers,
    PDWORD                     pdwNumUsersFound,
    PVOID**                    pppUserInfoList
    )
{
    DWORD dwError = 0;
    DWORD dwNumUsersAvailable = 0;
    DWORD dwNumUsersFound = 0;
    PLSA_USER_INFO_0* ppUserInfoList = NULL;
    DWORD iUser = 0;
    BOOLEAN bInLock = FALSE;

    LOCAL_LOCK_MUTEX(bInLock, &pEnumState->mutex);

    if (pEnumState->dwInfoLevel != 0)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwNumUsersAvailable = pEnumState->dwNumEntries - pEnumState->dwNextStartingId;

    dwNumUsersFound = LSA_MIN(dwNumMaxUsers, dwNumUsersAvailable);

    if (!dwNumUsersFound)
    {
        dwError = LW_ERROR_NO_MORE_USERS;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LwAllocateMemory(
                    dwNumUsersFound * sizeof(PLSA_USER_INFO_0),
                    (PVOID*)&ppUserInfoList);
    BAIL_ON_LSA_ERROR(dwError);

    for (; iUser < dwNumUsersFound; iUser++)
    {
        PDIRECTORY_ENTRY pEntry = NULL;

        pEntry = &pEnumState->pEntries[pEnumState->dwNextStartingId + iUser];

        dwError = LocalMarshalEntryToUserInfo_0(
                        pEntry,
                        NULL,
                        &ppUserInfoList[iUser]);
        BAIL_ON_LSA_ERROR(dwError);
    }

    pEnumState->dwNextStartingId += dwNumUsersFound;

    *pdwNumUsersFound = dwNumUsersFound;
    *pppUserInfoList = (PVOID*)ppUserInfoList;

cleanup:

    LOCAL_UNLOCK_MUTEX(bInLock, &pEnumState->mutex);

    return dwError;

error:

    *pdwNumUsersFound = 0;
    *pppUserInfoList = NULL;

    if (ppUserInfoList)
    {
        LsaFreeUserInfoList(
                pEnumState->dwInfoLevel,
                (PVOID*)ppUserInfoList,
                dwNumUsersFound);
    }

    goto cleanup;
}

static
DWORD
LocalDirEnumUsers_1(
    HANDLE                     hProvider,
    PLOCAL_PROVIDER_ENUM_STATE pEnumState,
    DWORD                      dwNumMaxUsers,
    PDWORD                     pdwNumUsersFound,
    PVOID**                    pppUserInfoList
    )
{
    DWORD dwError = 0;
    DWORD dwNumUsersAvailable = 0;
    DWORD dwNumUsersFound = 0;
    PLSA_USER_INFO_1* ppUserInfoList = NULL;
    DWORD iUser = 0;
    BOOLEAN bInLock = FALSE;

    LOCAL_LOCK_MUTEX(bInLock, &pEnumState->mutex);

    if (pEnumState->dwInfoLevel != 1)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwNumUsersAvailable = pEnumState->dwNumEntries - pEnumState->dwNextStartingId;

    dwNumUsersFound = LSA_MIN(dwNumMaxUsers, dwNumUsersAvailable);

    if (!dwNumUsersFound)
    {
        dwError = LW_ERROR_NO_MORE_USERS;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LwAllocateMemory(
                    dwNumUsersFound * sizeof(PLSA_USER_INFO_1),
                    (PVOID*)&ppUserInfoList);
    BAIL_ON_LSA_ERROR(dwError);

    for (; iUser < dwNumUsersFound; iUser++)
    {
        PDIRECTORY_ENTRY pEntry = NULL;

        pEntry = &pEnumState->pEntries[pEnumState->dwNextStartingId + iUser];

        dwError = LocalMarshalEntryToUserInfo_1(
                        pEntry,
                        gLPGlobals.pszLocalDomain,
                        NULL,
                        &ppUserInfoList[iUser]);
        BAIL_ON_LSA_ERROR(dwError);
    }

    pEnumState->dwNextStartingId += dwNumUsersFound;

    *pdwNumUsersFound = dwNumUsersFound;
    *pppUserInfoList = (PVOID*)ppUserInfoList;

cleanup:

    LOCAL_UNLOCK_MUTEX(bInLock, &pEnumState->mutex);

    return dwError;

error:

    *pdwNumUsersFound = 0;
    *pppUserInfoList = NULL;

    if (ppUserInfoList)
    {
        LsaFreeUserInfoList(
                pEnumState->dwInfoLevel,
                (PVOID*)ppUserInfoList,
                dwNumUsersFound);
    }

    goto cleanup;
}

static
DWORD
LocalDirEnumUsers_2(
    HANDLE                     hProvider,
    PLOCAL_PROVIDER_ENUM_STATE pEnumState,
    DWORD                      dwNumMaxUsers,
    PDWORD                     pdwNumUsersFound,
    PVOID**                    pppUserInfoList
    )
{
    DWORD dwError = 0;
    DWORD dwNumUsersAvailable = 0;
    DWORD dwNumUsersFound = 0;
    PLSA_USER_INFO_2* ppUserInfoList = NULL;
    DWORD iUser = 0;
    BOOLEAN bInLock = FALSE;

    LOCAL_LOCK_MUTEX(bInLock, &pEnumState->mutex);

    if (pEnumState->dwInfoLevel != 2)
    {
        dwError = LW_ERROR_DATA_ERROR;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwNumUsersAvailable = pEnumState->dwNumEntries - pEnumState->dwNextStartingId;

    dwNumUsersFound = LSA_MIN(dwNumMaxUsers, dwNumUsersAvailable);

    if (!dwNumUsersFound)
    {
        dwError = LW_ERROR_NO_MORE_USERS;
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LwAllocateMemory(
                    dwNumUsersFound * sizeof(PLSA_USER_INFO_2),
                    (PVOID*)&ppUserInfoList);
    BAIL_ON_LSA_ERROR(dwError);

    for (; iUser < dwNumUsersFound; iUser++)
    {
        PDIRECTORY_ENTRY pEntry = NULL;

        pEntry = &pEnumState->pEntries[pEnumState->dwNextStartingId + iUser];

        dwError = LocalMarshalEntryToUserInfo_2(
                        pEntry,
                        gLPGlobals.pszLocalDomain,
                        NULL,
                        &ppUserInfoList[iUser]);
        BAIL_ON_LSA_ERROR(dwError);
    }

    pEnumState->dwNextStartingId += dwNumUsersFound;

    *pdwNumUsersFound = dwNumUsersFound;
    *pppUserInfoList = (PVOID*)ppUserInfoList;

cleanup:

    LOCAL_UNLOCK_MUTEX(bInLock, &pEnumState->mutex);

    return dwError;

error:

    *pdwNumUsersFound = 0;
    *pppUserInfoList = NULL;

    if (ppUserInfoList)
    {
        LsaFreeUserInfoList(
                pEnumState->dwInfoLevel,
                (PVOID*)ppUserInfoList,
                dwNumUsersFound);
    }

    goto cleanup;
}

DWORD
LocalDirAddUser(
    HANDLE hProvider,
    DWORD  dwInfoLevel,
    PVOID  pUserInfo
    )
{
    DWORD dwError = 0;

    switch(dwInfoLevel)
    {
        case 0:

            dwError = LocalDirAddUser_0(
                            hProvider,
                            (PLSA_USER_INFO_0)pUserInfo);
            break;

        case 1:

            dwError = LocalDirAddUser_1(
                            hProvider,
                            (PLSA_USER_INFO_1)pUserInfo);

            break;

        case 2:

            dwError = LocalDirAddUser_2(
                            hProvider,
                            (PLSA_USER_INFO_2)pUserInfo);

            break;

        default:

            dwError = LW_ERROR_UNSUPPORTED_USER_LEVEL;

            break;
    }
    BAIL_ON_LSA_ERROR(dwError);

cleanup:

    return dwError;

error:

    goto cleanup;
}

DWORD
LocalDirAddUser_0(
    HANDLE           hProvider,
    PLSA_USER_INFO_0 pUserInfo
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    BOOLEAN bEventlogEnabled = FALSE;
    PLSA_LOGIN_NAME_INFO pLoginInfo = NULL;
    PWSTR pwszUserDN = NULL;
    enum AttrValueIndex {
        LOCAL_DAU0_IDX_GID = 0,
        LOCAL_DAU0_IDX_OBJECTCLASS,
        LOCAL_DAU0_IDX_SAM_ACCOUNT_NAME,
        LOCAL_DAU0_IDX_COMMON_NAME,
        LOCAL_DAU0_IDX_GECOS,
        LOCAL_DAU0_IDX_SHELL,
        LOCAL_DAU0_IDX_HOMEDIR,
        LOCAL_DAU0_IDX_OBJECTSID,
        LOCAL_DAU0_IDX_DOMAIN,
        LOCAL_DAU0_IDX_NETBIOS_DOMAIN,
        LOCAL_DAU0_IDX_UID,
        LOCAL_DAU0_IDX_ACCOUNT_FLAGS,
        LOCAL_DAU0_IDX_SENTINEL
    };
    ATTRIBUTE_VALUE attrValues[] =
    {
            {       /* LOCAL_DIR_ADD_USER_0_IDX_GID */
                    .Type = DIRECTORY_ATTR_TYPE_INTEGER,
                    .data.ulValue = pUserInfo->gid
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_OBJECTCLASS */
                    .Type = DIRECTORY_ATTR_TYPE_INTEGER,
                    .data.ulValue = LOCAL_OBJECT_CLASS_USER
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_SAM_ACCOUNT_NAME */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_COMMON_NAME */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_GECOS */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_SHELL */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {        /* LOCAL_DIR_ADD_USER_0_IDX_HOMEDIR */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_OBJECTSID */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_DOMAIN */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_NETBIOS_DOMAIN */
                    .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
                    .data.pwszStringValue = NULL
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_UID */
                    .Type = DIRECTORY_ATTR_TYPE_INTEGER,
                    .data.ulValue = pUserInfo->uid
            },
            {       /* LOCAL_DIR_ADD_USER_0_IDX_ACCOUNT_FLAGS */
                    .Type = DIRECTORY_ATTR_TYPE_INTEGER,
                    .data.ulValue = 0
            }
    };
    WCHAR wszAttrObjectClass[]       = LOCAL_DIR_ATTR_OBJECT_CLASS;
    WCHAR wszAttrDistinguishedName[] = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    WCHAR wszAttrObjectSID[]         = LOCAL_DIR_ATTR_OBJECT_SID;
    WCHAR wszAttrGID[]               = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    WCHAR wszAttrSamAccountName[]    = LOCAL_DIR_ATTR_SAM_ACCOUNT_NAME;
    WCHAR wszAttrCommonName[]        = LOCAL_DIR_ATTR_COMMON_NAME;
    WCHAR wszAttrGecos[]             = LOCAL_DIR_ATTR_GECOS;
    WCHAR wszAttrShell[]             = LOCAL_DIR_ATTR_SHELL;
    WCHAR wszAttrHomedir[]           = LOCAL_DIR_ATTR_HOME_DIR;
    WCHAR wszAttrDomain[]            = LOCAL_DIR_ATTR_DOMAIN;
    WCHAR wszAttrNameNetBIOSDomain[] = LOCAL_DIR_ATTR_NETBIOS_NAME;
    WCHAR wszAttrNameUID[]           = LOCAL_DIR_ATTR_UID;
    WCHAR wszAttrNameAccountFlags[]  = LOCAL_DIR_ATTR_ACCOUNT_FLAGS;
    DIRECTORY_MOD modGID =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrGID[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_GID]
    };
    DIRECTORY_MOD modObjectClass =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrObjectClass[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_OBJECTCLASS]
    };
    DIRECTORY_MOD modSamAccountName =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrSamAccountName[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_SAM_ACCOUNT_NAME]
    };
    DIRECTORY_MOD modCommonName =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrCommonName[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_COMMON_NAME]
    };
    DIRECTORY_MOD modGecos =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrGecos[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_GECOS]
    };
    DIRECTORY_MOD modShell =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrShell[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_SHELL]
    };
    DIRECTORY_MOD modHomedir =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrHomedir[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_HOMEDIR]
    };
    DIRECTORY_MOD modDomain =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrDomain[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_DOMAIN]
    };
    DIRECTORY_MOD modNetBIOSDomain =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrNameNetBIOSDomain[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_NETBIOS_DOMAIN]
    };
    DIRECTORY_MOD modUID =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrNameUID[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_UID]
    };
    DIRECTORY_MOD modAcctFlags =
    {
        DIR_MOD_FLAGS_ADD,
        &wszAttrNameAccountFlags[0],
        1,
        &attrValues[LOCAL_DAU0_IDX_ACCOUNT_FLAGS]
    };
    DIRECTORY_MOD mods[LOCAL_DAU0_IDX_SENTINEL + 1];

    DWORD iMod = 0;
    PWSTR pwszSamAccountName = NULL;
    PWSTR pwszGecos = NULL;
    PWSTR pwszShell = NULL;
    PSTR  pszShell  = NULL;
    PWSTR pwszHomedir = NULL;
    PSTR  pszHomedir = NULL;
    PCSTR pszPassword = "";
    PWSTR pwszPassword = NULL;
    PWSTR pwszDomain        = NULL;
    PWSTR pwszNetBIOSDomain = NULL;
    PLSA_GROUP_INFO_0 pGroupInfo = NULL;
    DWORD dwGroupInfoLevel = 0;
    PWSTR pwszGroupDN = NULL;
    PWSTR pwszBase = NULL;
    ULONG ulScope = 0;
    wchar_t wszFilterFmt[] = L"%ws = \'%ws\'";
    PWSTR pwszFilter = NULL;
    DWORD dwFilterLen = 0;
    PDIRECTORY_ENTRY pMember = NULL;
    DWORD dwNumEntries = 0;
    BOOLEAN bUserAdded = FALSE;
    gid_t gid = 0;
    PSID pGroupSID = NULL;

    PWSTR wszMemberAttrs[] = {
        wszAttrObjectClass,
        wszAttrDistinguishedName,
        wszAttrObjectSID,
        wszAttrGID,
        NULL
    };

    memset(&mods[0], 0, sizeof(mods));

    BAIL_ON_INVALID_STRING(pUserInfo->pszName);

    if (pUserInfo->uid) {
        dwError = LocalDirValidateUID(pUserInfo->uid);
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LocalCrackDomainQualifiedName(
                    pUserInfo->pszName,
                    &pLoginInfo);
    BAIL_ON_LSA_ERROR(dwError);

    if (!LocalServicesDomain(pLoginInfo->pszDomainNetBiosName))
    {
        dwError = LW_ERROR_NOT_HANDLED;
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserInfo->gid) {
        dwError = LocalDirFindGroupById(
                        hProvider,
                        pUserInfo->gid,
                        dwGroupInfoLevel,
                        &pwszGroupDN,
                        (PVOID*)&pGroupInfo);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = RtlAllocateSidFromCString(
                        &pGroupSID,
                        pGroupInfo->pszSid);
        BAIL_ON_LSA_ERROR(dwError);

        if (!RtlIsPrefixSid(
                        gLPGlobals.pLocalDomainSID,
                        pGroupSID))
        {
            dwError = LW_ERROR_INVALID_GROUP;
            BAIL_ON_LSA_ERROR(dwError);
        }
    }

    dwError = LsaMbsToWc16s(
                    pLoginInfo->pszFullDomainName,
                    &pwszDomain);
    BAIL_ON_LSA_ERROR(dwError);

    attrValues[LOCAL_DAU0_IDX_DOMAIN].data.pwszStringValue = pwszDomain;

    dwError = LsaMbsToWc16s(
                    pLoginInfo->pszDomainNetBiosName,
                    &pwszNetBIOSDomain);
    BAIL_ON_LSA_ERROR(dwError);

    attrValues[LOCAL_DAU0_IDX_NETBIOS_DOMAIN].data.pwszStringValue = pwszNetBIOSDomain;

    dwError = LsaMbsToWc16s(
                    pUserInfo->pszName,
                    &pwszSamAccountName);
    BAIL_ON_LSA_ERROR(dwError);

    attrValues[LOCAL_DAU0_IDX_SAM_ACCOUNT_NAME].data.pwszStringValue = pwszSamAccountName;

    attrValues[LOCAL_DAU0_IDX_COMMON_NAME].data.pwszStringValue = pwszSamAccountName;

    dwError = LocalBuildDN(
                    pLoginInfo,
                    &pwszUserDN);
    BAIL_ON_LSA_ERROR(dwError);

    if (!LW_IS_NULL_OR_EMPTY_STR(pUserInfo->pszGecos))
    {
        dwError = LsaMbsToWc16s(
                    pUserInfo->pszGecos,
                    &pwszGecos);
        BAIL_ON_LSA_ERROR(dwError);

        attrValues[LOCAL_DAU0_IDX_GECOS].data.pwszStringValue = pwszGecos;
    }

    if (!LW_IS_NULL_OR_EMPTY_STR(pUserInfo->pszHomedir))
    {
        dwError = LsaMbsToWc16s(
                    pUserInfo->pszHomedir,
                    &pwszHomedir);
        BAIL_ON_LSA_ERROR(dwError);
    }
    else
    {
        dwError = LocalBuildHomeDirPathFromTemplate(
                        pUserInfo->pszName,
                        pLoginInfo->pszDomainNetBiosName,
                        &pszHomedir);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = LsaMbsToWc16s(
                        pszHomedir,
                        &pwszHomedir);
        BAIL_ON_LSA_ERROR(dwError);
    }

    attrValues[LOCAL_DAU0_IDX_HOMEDIR].data.pwszStringValue = pwszHomedir;

    if (!LW_IS_NULL_OR_EMPTY_STR(pUserInfo->pszShell))
    {
        dwError = LsaMbsToWc16s(
                    pUserInfo->pszShell,
                    &pwszShell);
        BAIL_ON_LSA_ERROR(dwError);
    }
    else
    {
        dwError = LocalCfgGetDefaultShell(&pszShell);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = LsaMbsToWc16s(
                    pUserInfo->pszShell,
                    &pwszShell);
        BAIL_ON_LSA_ERROR(dwError);
    }

    attrValues[LOCAL_DAU0_IDX_SHELL].data.pwszStringValue = pwszShell;

    attrValues[LOCAL_DAU0_IDX_ACCOUNT_FLAGS].data.ulValue = LOCAL_ACB_NORMAL;

    if (LW_IS_NULL_OR_EMPTY_STR(pUserInfo->pszPasswd))
    {
        attrValues[LOCAL_DAU0_IDX_ACCOUNT_FLAGS].data.ulValue |= LOCAL_ACB_DISABLED;
    }

    mods[iMod++] = modObjectClass;
    if (pUserInfo->uid)
    {
        mods[iMod++] = modUID;
    }
    if (pUserInfo->gid)
    {
        mods[iMod++] = modGID;
    }
    mods[iMod++] = modSamAccountName;
    mods[iMod++] = modCommonName;
    mods[iMod++] = modGecos;
    mods[iMod++] = modShell;
    mods[iMod++] = modHomedir;
    mods[iMod++] = modDomain;
    mods[iMod++] = modNetBIOSDomain;
    mods[iMod++] = modAcctFlags;

    dwError = DirectoryAddObject(
                    pContext->hDirectory,
                    pwszUserDN,
                    mods);
    BAIL_ON_LSA_ERROR(dwError);

    bUserAdded = TRUE;

    dwFilterLen = (sizeof(wszAttrDistinguishedName) - 2) +
                  (wc16slen(pwszUserDN) * sizeof(WCHAR)) +
                   sizeof(wszFilterFmt);

    dwError = LwAllocateMemory(
                    dwFilterLen,
                    (PVOID*)&pwszFilter);
    BAIL_ON_LSA_ERROR(dwError);

    sw16printfw(pwszFilter, dwFilterLen/sizeof(WCHAR), wszFilterFmt,
                wszAttrDistinguishedName, pwszUserDN);

    dwError = DirectorySearch(
                    pContext->hDirectory,
                    pwszBase,
                    ulScope,
                    pwszFilter,
                    wszMemberAttrs,
                    0,
                    &pMember,
                    &dwNumEntries);
    BAIL_ON_LSA_ERROR(dwError);

    if (!pwszGroupDN) {
        dwError = DirectoryGetEntryAttrValueByName(
                        pMember,
                        wszAttrGID,
                        DIRECTORY_ATTR_TYPE_INTEGER,
                        &gid);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = LocalDirFindGroupById(
                        hProvider,
                        gid,
                        dwGroupInfoLevel,
                        &pwszGroupDN,
                        (PVOID*)&pGroupInfo);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = RtlAllocateSidFromCString(
                        &pGroupSID,
                        pGroupInfo->pszSid);
        BAIL_ON_LSA_ERROR(dwError);

        if (!RtlIsPrefixSid(
                        gLPGlobals.pLocalDomainSID,
                        pGroupSID))
        {
            dwError = LW_ERROR_SAM_DATABASE_ERROR;
            BAIL_ON_LSA_ERROR(dwError);
        }
    }

    dwError = DirectoryAddToGroup(
                    pContext->hDirectory,
                    pwszGroupDN,
                    pMember);
    BAIL_ON_LSA_ERROR(dwError);

    if (pUserInfo->pszPasswd)
    {
        pszPassword = pUserInfo->pszPasswd;
    }

    dwError = LsaMbsToWc16s(
                    pszPassword,
                    &pwszPassword);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = DirectorySetPassword(
                    pContext->hDirectory,
                    pwszUserDN,
                    pwszPassword);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LocalCfgIsEventlogEnabled(&bEventlogEnabled);
    BAIL_ON_LSA_ERROR(dwError);

    if (bEventlogEnabled)
    {
        LocalEventLogUserAdd(pLoginInfo->pszName,
                             ((PLSA_USER_INFO_0)pUserInfo)->uid);
    }

cleanup:

    if (pLoginInfo)
    {
        LsaFreeNameInfo(pLoginInfo);
    }

    if (pGroupInfo)
    {
        LsaFreeGroupInfo(dwGroupInfoLevel, pGroupInfo);
    }

    LW_SAFE_FREE_MEMORY(pwszUserDN);
    LW_SAFE_FREE_MEMORY(pwszGroupDN);
    LW_SAFE_FREE_MEMORY(pwszSamAccountName);
    LW_SAFE_FREE_MEMORY(pwszGecos);
    LW_SAFE_FREE_MEMORY(pwszShell);
    LW_SAFE_FREE_STRING(pszShell);
    LW_SAFE_FREE_MEMORY(pwszHomedir);
    LW_SAFE_FREE_STRING(pszHomedir);
    LW_SAFE_FREE_MEMORY(pwszPassword);
    LW_SAFE_FREE_MEMORY(pwszDomain);
    LW_SAFE_FREE_MEMORY(pwszNetBIOSDomain);
    RTL_FREE(&pGroupSID);

    LW_SAFE_FREE_MEMORY(pwszFilter);

    if (pMember)
    {
        DirectoryFreeEntries(pMember, dwNumEntries);
    }

    return dwError;

error:

    if (bUserAdded)
    {
        DWORD dwError2 = 0;

        dwError2 = DirectoryDeleteObject(
                        pContext->hDirectory,
                        pwszUserDN);
        if (dwError2)
        {
            LSA_LOG_ERROR("Failed to remove user [code: %d]", dwError2);
        }
    }

    goto cleanup;
}

DWORD
LocalDirAddUser_1(
    HANDLE           hProvider,
    PLSA_USER_INFO_1 pUserInfo
    )
{
    return LW_ERROR_NOT_SUPPORTED;
}

DWORD
LocalDirAddUser_2(
    HANDLE           hProvider,
    PLSA_USER_INFO_2 pUserInfo
    )
{
    return LW_ERROR_NOT_SUPPORTED;
}

static
DWORD
LocalDirValidateUID(
    uid_t uid
    )
{
    DWORD dwError = 0;

    /* Check whether account uid is within permitted range */
    if (uid < LOWEST_UID) {
        dwError = LW_ERROR_INVALID_PARAMETER;
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:
    return dwError;

error:
    goto cleanup;
}


DWORD
LocalDirModifyUser(
    HANDLE             hProvider,
    PLSA_USER_MOD_INFO pUserModInfo
    )
{
    DWORD   dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    DWORD   dwUserInfoLevel = 2;
    PWSTR   pwszUserDN = NULL;
    PLSA_USER_INFO_2 pUserInfo = NULL;
    DWORD   dwUserInfoFlags = 0;
    DWORD   dwOrigUserInfoFlags = 0;
    WCHAR   wszAttrDN[] = LOCAL_DIR_ATTR_DISTINGUISHED_NAME;
    DIRECTORY_MOD mods[9] = {{0}};

    ATTRIBUTE_VALUE UserAttrVal[] = {
        {
            .Type = DIRECTORY_ATTR_TYPE_UNICODE_STRING,
            .data.pwszStringValue = NULL
        }
    };

    DIRECTORY_ATTRIBUTE UserAttr[] = {
        {
            /* placeholder for DN */
            .pwszName    = wszAttrDN,
            .ulNumValues = 1,
            .pValues     = UserAttrVal
        },
        {
            /* termination */
            NULL,
            0,
            NULL
        }
    };

    DIRECTORY_ENTRY UserEntry[] = {
        { 1, UserAttr },
        { 0, NULL }
    };

    WCHAR wszAttrNameUserInfoFlags[] = LOCAL_DIR_ATTR_ACCOUNT_FLAGS;
    WCHAR wszAttrNameAccountExpiry[] = LOCAL_DIR_ATTR_ACCOUNT_EXPIRY;
    WCHAR wszAttrNameNtHash [] = LOCAL_DIR_ATTR_NT_HASH;
    WCHAR wszAttrNameLmHash [] = LOCAL_DIR_ATTR_LM_HASH;
    WCHAR wszAttrNamePrimaryGroup [] = LOCAL_DIR_ATTR_PRIMARY_GROUP;
    WCHAR wszAttrNameShell [] = LOCAL_DIR_ATTR_SHELL;
    WCHAR wszAttrNameGecos [] = LOCAL_DIR_ATTR_GECOS;
    WCHAR wszAttrNameHomedir [] = LOCAL_DIR_ATTR_HOME_DIR;
    ATTRIBUTE_VALUE avUserInfoFlags = {0};
    ATTRIBUTE_VALUE avAccountExpiry = {0};
    ATTRIBUTE_VALUE avNtHash = {0};
    ATTRIBUTE_VALUE avLmHash = {0};
    ATTRIBUTE_VALUE avPrimaryGroup = {0};
    ATTRIBUTE_VALUE avShell = {0};
    ATTRIBUTE_VALUE avGecos = {0};
    ATTRIBUTE_VALUE avHomedir = {0};
    DWORD             dwNumMods = 0;
    DWORD             dwGroupInfoLevel = 0;
    PWSTR             pwszGroupDN_remove = NULL;
    PLSA_GROUP_INFO_0 pGroupInfo_remove = NULL;
    PWSTR             pwszGroupDN_add   = NULL;
    PLSA_GROUP_INFO_0 pGroupInfo_add    = NULL;
    PWSTR             pwszPassword      = NULL;
    OCTET_STRING NtHashBlob = {0};
    OCTET_STRING LmHashBlob = {0};

    dwError = LocalDirFindUserById(
                    hProvider,
                    pUserModInfo->uid,
                    dwUserInfoLevel,
                    &pwszUserDN,
                    (PVOID*)&pUserInfo);
    BAIL_ON_LSA_ERROR(dwError);

    UserAttr[0].pValues[0].data.pwszStringValue = pwszUserDN;

    dwError = LocalDirGetUserInfoFlags(
                    hProvider,
                    pUserModInfo->uid,
                    &dwOrigUserInfoFlags);
    BAIL_ON_LSA_ERROR(dwError);

    dwUserInfoFlags = dwOrigUserInfoFlags;

    if (pUserModInfo->actions.bEnableUser)
    {
        dwUserInfoFlags &= ~LOCAL_ACB_DISABLED;
    }
    else if (pUserModInfo->actions.bDisableUser)
    {
        dwUserInfoFlags |= LOCAL_ACB_DISABLED;
    }

    if (pUserModInfo->actions.bSetPasswordMustExpire)
    {
        dwUserInfoFlags &= ~LOCAL_ACB_PWNOEXP;
    }
    else if (pUserModInfo->actions.bSetPasswordNeverExpires)
    {
        dwUserInfoFlags |= LOCAL_ACB_PWNOEXP;
    }

    if (pUserModInfo->actions.bSetChangePasswordOnNextLogon)
    {
        dwUserInfoFlags |= LOCAL_ACB_PW_EXPIRED;
    }

    if (pUserModInfo->actions.bUnlockUser)
    {
        dwUserInfoFlags &= ~LOCAL_ACB_NORMAL;
    }

    if (dwUserInfoFlags != dwOrigUserInfoFlags)
    {
        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameUserInfoFlags[0];
        mods[dwNumMods].ulNumValues = 1;
        avUserInfoFlags.Type = DIRECTORY_ATTR_TYPE_INTEGER;
        avUserInfoFlags.data.ulValue = dwUserInfoFlags;
        mods[dwNumMods].pAttrValues = &avUserInfoFlags;

        dwNumMods++;
    }

    if (pUserModInfo->actions.bSetAccountExpiryDate)
    {
        struct tm tmbuf = {0};

        if (strptime(pUserModInfo->pszExpiryDate, "%Y-%m-%d", &tmbuf) == NULL)
        {
            dwError = errno;
            BAIL_ON_LSA_ERROR(dwError);
        }

        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameAccountExpiry[0];
        mods[dwNumMods].ulNumValues = 1;
        avAccountExpiry.Type = DIRECTORY_ATTR_TYPE_LARGE_INTEGER;
        avAccountExpiry.data.llValue = LocalGetNTTime(mktime(&tmbuf));
        mods[dwNumMods].pAttrValues = &avAccountExpiry;

        dwNumMods++;
    }

    if (pUserModInfo->actions.bSetNtPasswordHash)
    {
        NtHashBlob.ulNumBytes = pUserModInfo->pNtPasswordHash->dwLen;
        NtHashBlob.pBytes     = pUserModInfo->pNtPasswordHash->pData;

        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameNtHash[0];
        mods[dwNumMods].ulNumValues = 1;
        avNtHash.Type = DIRECTORY_ATTR_TYPE_OCTET_STREAM;
        avNtHash.data.pOctetString = &NtHashBlob;
        mods[dwNumMods].pAttrValues = &avNtHash;

        dwNumMods++;
    }

    if (pUserModInfo->actions.bSetLmPasswordHash)
    {
        LmHashBlob.ulNumBytes = pUserModInfo->pLmPasswordHash->dwLen;
        LmHashBlob.pBytes     = pUserModInfo->pLmPasswordHash->pData;

        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameLmHash[0];
        mods[dwNumMods].ulNumValues = 1;
        avLmHash.Type = DIRECTORY_ATTR_TYPE_OCTET_STREAM;
        avLmHash.data.pOctetString = &LmHashBlob;
        mods[dwNumMods].pAttrValues = &avLmHash;

        dwNumMods++;
    }

    if (pUserModInfo->actions.bSetPrimaryGroup)
    {
        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNamePrimaryGroup[0];
        mods[dwNumMods].ulNumValues = 1;
        avPrimaryGroup.Type = DIRECTORY_ATTR_TYPE_INTEGER;
        avPrimaryGroup.data.ulValue = pUserModInfo->gid;
        mods[dwNumMods].pAttrValues = &avPrimaryGroup;

        dwNumMods++;
    }
    if (pUserModInfo->actions.bSetShell)
    {
        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameShell[0];
        mods[dwNumMods].ulNumValues = 1;
        avShell.Type = DIRECTORY_ATTR_TYPE_ANSI_STRING;
        avShell.data.pszStringValue = pUserModInfo->pszShell;
        mods[dwNumMods].pAttrValues = &avShell;

        dwNumMods++;
    }
    if (pUserModInfo->actions.bSetGecos)
    {
        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameGecos[0];
        mods[dwNumMods].ulNumValues = 1;
        avGecos.Type = DIRECTORY_ATTR_TYPE_ANSI_STRING;
        avGecos.data.pszStringValue = pUserModInfo->pszGecos;
        mods[dwNumMods].pAttrValues = &avGecos;

        dwNumMods++;
    }
    if (pUserModInfo->actions.bSetHomedir)
    {
        mods[dwNumMods].ulOperationFlags = DIR_MOD_FLAGS_REPLACE;
        mods[dwNumMods].pwszAttrName = &wszAttrNameHomedir[0];
        mods[dwNumMods].ulNumValues = 1;
        avHomedir.Type = DIRECTORY_ATTR_TYPE_ANSI_STRING;
        avHomedir.data.pszStringValue = pUserModInfo->pszHomedir;
        mods[dwNumMods].pAttrValues = &avHomedir;

        dwNumMods++;
    }


    if (dwNumMods)
    {
        dwError = DirectoryModifyObject(
                        pContext->hDirectory,
                        pwszUserDN,
                        mods);
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserModInfo->actions.bRemoveFromGroups)
    {
        dwError = LocalFindGroupByNameEx(
                        hProvider,
                        pUserModInfo->pszRemoveFromGroups,
                        0,
                        dwGroupInfoLevel,
                        &pwszGroupDN_remove,
                        (PVOID*)&pGroupInfo_remove);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = DirectoryRemoveFromGroup(
                        pContext->hDirectory,
                        pwszGroupDN_remove,
                        UserEntry);
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserModInfo->actions.bAddToGroups)
    {
        dwError = LocalFindGroupByNameEx(
                        hProvider,
                        pUserModInfo->pszAddToGroups,
                        0,
                        dwGroupInfoLevel,
                        &pwszGroupDN_add,
                        (PVOID*)&pGroupInfo_add);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = DirectoryAddToGroup(
                        pContext->hDirectory,
                        pwszGroupDN_add,
                        UserEntry);
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserModInfo->actions.bSetPassword)
    {
        dwError = LsaMbsToWc16s(
                        pUserModInfo->pszPassword ? pUserModInfo->pszPassword : "",
                        &pwszPassword);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = LocalDirSetPassword(
                        hProvider,
                        pwszUserDN,
                        pwszPassword);
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:

    if (pUserInfo)
    {
        LsaFreeUserInfo(dwUserInfoLevel, pUserInfo);
    }

    LW_SAFE_FREE_MEMORY(pwszGroupDN_remove);
    if (pGroupInfo_remove)
    {
        LsaFreeGroupInfo(dwGroupInfoLevel, pGroupInfo_remove);
    }

    LW_SAFE_FREE_MEMORY(pwszGroupDN_add);
    if (pGroupInfo_add)
    {
        LsaFreeGroupInfo(dwGroupInfoLevel, pGroupInfo_add);
    }

    LW_SAFE_FREE_MEMORY(pwszPassword);

    LW_SAFE_FREE_MEMORY(pwszUserDN);

    return dwError;

error:

    goto cleanup;
}

DWORD
LocalDirDeleteUser(
    HANDLE hProvider,
    PWSTR  pwszUserDN
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;

    dwError = DirectoryDeleteObject(
                    pContext->hDirectory,
                    pwszUserDN);
    BAIL_ON_LSA_ERROR(dwError);

error:

    return dwError;
}

DWORD
LocalDirChangePassword(
    HANDLE hProvider,
    PWSTR  pwszUserDN,
    PWSTR  pwszOldPassword,
    PWSTR  pwszNewPassword
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;

    dwError = DirectoryChangePassword(
                    pContext->hDirectory,
                    pwszUserDN,
                    pwszOldPassword,
                    pwszNewPassword);
    BAIL_ON_LSA_ERROR(dwError);

error:

    return dwError;
}

DWORD
LocalDirSetPassword(
    HANDLE hProvider,
    PWSTR pwszUserDN,
    PWSTR pwszNewPassword
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    BOOLEAN bIsAdmin = FALSE;

    dwError = LocalCheckIsAdministrator(
                    hProvider,
                    &bIsAdmin);
    BAIL_ON_LSA_ERROR(dwError);

    if (bIsAdmin)
    {
        dwError = DirectorySetPassword(
                        pContext->hDirectory,
                        pwszUserDN,
                        pwszNewPassword);
        BAIL_ON_LSA_ERROR(dwError);
    }
    else
    {
        dwError = EACCES;
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:
    return dwError;

error:
    goto cleanup;
}

DWORD
LocalCreateHomeDirectory(
    PLSA_USER_INFO_0 pUserInfo
    )
{
    DWORD dwError = 0;
    BOOLEAN bExists = FALSE;
    mode_t  umask = LOCAL_CFG_DEFAULT_HOMEDIR_UMASK;
    mode_t  perms = (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    BOOLEAN bRemoveDir = FALSE;

    if (LW_IS_NULL_OR_EMPTY_STR(pUserInfo->pszHomedir))
    {
       LSA_LOG_ERROR("The user's [Uid:%ld] home directory is not defined",
                     (long)pUserInfo->uid);
       dwError = LW_ERROR_FAILED_CREATE_HOMEDIR;
       BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LsaCheckDirectoryExists(
                    pUserInfo->pszHomedir,
                    &bExists);
    BAIL_ON_LSA_ERROR(dwError);

    if (!bExists)
    {
        dwError = LocalCfgGetHomedirUmask(&umask);
        BAIL_ON_LSA_ERROR(dwError);

        dwError = LsaCreateDirectory(
                    pUserInfo->pszHomedir,
                    perms & (~umask));
        BAIL_ON_LSA_ERROR(dwError);

        bRemoveDir = TRUE;

        dwError = LsaChangeOwner(
                    pUserInfo->pszHomedir,
                    pUserInfo->uid,
                    pUserInfo->gid);
        BAIL_ON_LSA_ERROR(dwError);

        bRemoveDir = FALSE;

        dwError = LocalProvisionHomeDir(
                       pUserInfo->uid,
                       pUserInfo->gid,
                       pUserInfo->pszHomedir);
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:

    return dwError;

error:

    if (bRemoveDir) {
       LsaRemoveDirectory(pUserInfo->pszHomedir);
    }

    goto cleanup;
}

DWORD
LocalProvisionHomeDir(
    uid_t ownerUid,
    gid_t ownerGid,
    PCSTR pszHomedirPath
    )
{
    DWORD dwError = 0;
    BOOLEAN bExists = FALSE;
    PSTR pszSkelPaths = NULL;
    PSTR pszSkelPath = NULL;
    PSTR pszIter = NULL;
    size_t stLen = 0;

    dwError = LocalCfgGetSkeletonDirs(&pszSkelPaths);
    BAIL_ON_LSA_ERROR(dwError);

    if (LW_IS_NULL_OR_EMPTY_STR(pszSkelPaths))
    {
        goto cleanup;
    }

    pszIter = pszSkelPaths;
    while ((stLen = strcspn(pszIter, ",")) != 0)
    {
        dwError = LwStrndup(
                      pszIter,
                      stLen,
                      &pszSkelPath);
        BAIL_ON_LSA_ERROR(dwError);

        LwStripWhitespace(pszSkelPath, TRUE, TRUE);

        if (LW_IS_NULL_OR_EMPTY_STR(pszSkelPath))
        {
            LW_SAFE_FREE_STRING(pszSkelPath);
            continue;
        }

        dwError = LsaCheckDirectoryExists(
                        pszSkelPath,
                        &bExists);
        BAIL_ON_LSA_ERROR(dwError);

        if (bExists)
        {
            dwError = LsaCopyDirectory(
                        pszSkelPath,
                        ownerUid,
                        ownerGid,
                        pszHomedirPath);
            BAIL_ON_LSA_ERROR(dwError);
        }

        LW_SAFE_FREE_STRING(pszSkelPath);

        pszIter += stLen;
        stLen = strspn(pszIter, ",");
        pszIter += stLen;
    }

cleanup:

    LW_SAFE_FREE_STRING(pszSkelPath);
    LW_SAFE_FREE_STRING(pszSkelPaths);

    return dwError;

error:

    goto cleanup;
}

DWORD
LocalCheckAccountFlags(
    PLSA_USER_INFO_2 pUserInfo2
    )
{
    DWORD dwError = LW_ERROR_SUCCESS;

    BAIL_ON_INVALID_POINTER(pUserInfo2);

    if (pUserInfo2->bAccountDisabled)
    {
        dwError = LW_ERROR_ACCOUNT_DISABLED;
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserInfo2->bAccountLocked)
    {
        dwError = LW_ERROR_ACCOUNT_LOCKED;
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserInfo2->bAccountExpired)
    {
        dwError = LW_ERROR_ACCOUNT_EXPIRED;
        BAIL_ON_LSA_ERROR(dwError);
    }

    if (pUserInfo2->bPasswordExpired)
    {
        dwError = LW_ERROR_PASSWORD_EXPIRED;
        BAIL_ON_LSA_ERROR(dwError);
    }

error:

    return dwError;
}

DWORD
LocalUpdateUserLoginTime(
    HANDLE hProvider,
    PWSTR  pwszUserDN
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    ATTRIBUTE_VALUE attrValue =
    {
        .Type = DIRECTORY_ATTR_TYPE_LARGE_INTEGER,
        .data.llValue = 0
    };
    WCHAR wszAttrNameLastLogonTime[] = LOCAL_DIR_ATTR_LAST_LOGON;
    DIRECTORY_MOD mods[] =
    {
        {
            DIR_MOD_FLAGS_REPLACE,
            &wszAttrNameLastLogonTime[0],
            1,
            &attrValue
        },
        {
            DIR_MOD_FLAGS_REPLACE,
            NULL,
            0,
            NULL
        }
    };

    attrValue.data.llValue = LocalGetNTTime(time(NULL));

    dwError = DirectoryModifyObject(
                    pContext->hDirectory,
                    pwszUserDN,
                    mods);
    BAIL_ON_LSA_ERROR(dwError);

cleanup:

    return dwError;

error:

    goto cleanup;
}

DWORD
LocalUpdateUserLogoffTime(
    HANDLE hProvider,
    PWSTR  pwszUserDN
    )
{
    DWORD dwError = 0;
    PLOCAL_PROVIDER_CONTEXT pContext = (PLOCAL_PROVIDER_CONTEXT)hProvider;
    ATTRIBUTE_VALUE attrValue =
    {
        .Type = DIRECTORY_ATTR_TYPE_LARGE_INTEGER,
        .data.llValue = 0
    };
    WCHAR wszAttrNameLastLogoffTime[] = LOCAL_DIR_ATTR_LAST_LOGOFF;
    DIRECTORY_MOD mods[] =
    {
        {
            DIR_MOD_FLAGS_REPLACE,
            &wszAttrNameLastLogoffTime[0],
            1,
            &attrValue
        },
        {
            DIR_MOD_FLAGS_ADD,
            NULL,
            0,
            NULL
        }
    };

    attrValue.data.llValue = LocalGetNTTime(time(NULL));

    dwError = DirectoryModifyObject(
                    pContext->hDirectory,
                    pwszUserDN,
                    mods);
    BAIL_ON_LSA_ERROR(dwError);

cleanup:

    return dwError;

error:

    goto cleanup;
}

DWORD
LocalDirCheckIfAdministrator(
    HANDLE   hProvider,
    uid_t    uid,
    PBOOLEAN pbIsAdmin
    )
{
    DWORD dwError = 0;
    DWORD dwGroupInfoLevel = 0;
    PLSA_GROUP_INFO_0* ppGroupInfoList = NULL;
    DWORD dwNumGroups = 0;
    BOOLEAN bIsAdmin = FALSE;

    if (uid == 0)
    {
        bIsAdmin = TRUE;
    }
    else
    {
        dwError = LocalGetGroupsForUser(
                        hProvider,
                        NULL,
                        uid,
                        0,
                        dwGroupInfoLevel,
                        &dwNumGroups,
                        (PVOID**)&ppGroupInfoList);
        if (dwError == LW_ERROR_NO_SUCH_USER)
        {
            dwError = LW_ERROR_SUCCESS;
        }
        BAIL_ON_LSA_ERROR(dwError);

        if (dwNumGroups)
        {
            DWORD iGroup = 0;
            gid_t gid_BuiltinAdmin = 0;

            dwError = LocalDirGetBuiltinAdministratorsGID(
                            hProvider,
                            &gid_BuiltinAdmin);
            BAIL_ON_LSA_ERROR(dwError);

            for (; iGroup < dwNumGroups; iGroup++)
            {
                PLSA_GROUP_INFO_0 pGroupInfo = ppGroupInfoList[iGroup];

                if (pGroupInfo->gid == gid_BuiltinAdmin)
                {
                    bIsAdmin = TRUE;

                    break;
                }
            }
        }
    }

    *pbIsAdmin = bIsAdmin;

cleanup:

    if (ppGroupInfoList)
    {
        LsaFreeGroupInfoList(dwGroupInfoLevel, (PVOID*)ppGroupInfoList, dwNumGroups);
    }

    return dwError;

error:

    *pbIsAdmin = FALSE;

    goto cleanup;
}

static
DWORD
LocalDirGetBuiltinAdministratorsGID(
    HANDLE hProvider,
    gid_t* pGID
    )
{
    DWORD dwError = 0;
    PLSA_GROUP_INFO_0 pGroupInfo = NULL;
    DWORD  dwInfoLevel = 0;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static gid_t gid_BuiltinAdmin = 0;

    pthread_mutex_lock(&mutex);

    if (!gid_BuiltinAdmin)
    {
        dwError = LocalDirFindGroupByName(
                        hProvider,
                        "BUILTIN",
                        "Administrators",
                        dwInfoLevel,
                        NULL,
                        (PVOID*)&pGroupInfo);
        BAIL_ON_LSA_ERROR(dwError);

        gid_BuiltinAdmin = pGroupInfo->gid;
    }

    *pGID = gid_BuiltinAdmin;

cleanup:

    if (pGroupInfo)
    {
        LsaFreeGroupInfo(dwInfoLevel, pGroupInfo);
        pGroupInfo = NULL;
    }

    pthread_mutex_unlock(&mutex);

    return dwError;

error:

    *pGID = 0;

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
