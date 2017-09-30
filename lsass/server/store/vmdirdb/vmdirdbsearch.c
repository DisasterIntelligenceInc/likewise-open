/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software
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
 *        samdbsearch.c
 *
 * Abstract:
 *
 *
 *      Likewise VMDIR Database Provider
 *
 *      VMDIR objects searching routines
 *
 * Authors: Adam Bernstein (abernstein@vmware.com)
 *
 */

#include "includes.h"

/*
 * Each directory entry has a count of these number of attributes and values
 */
typedef struct _DIRECTORY_ATTRIBUTE_VALUE_COUNT
{
    DWORD iAttributes;
} DIRECTORY_ATTRIBUTE_VALUE_COUNT, *PDIRECTORY_ATTRIBUTE_VALUE_COUNT;

static DWORD
VmdirDbCountEntriesAndAttributes(
    LDAP *pLd,
    LDAPMessage *pRes,
    PSTR *ppszAttributes,
    DWORD *pdwNumEntries, /* Number of directory entries */
    PDWORD *ppdwAttributes)
{
    DWORD dwError = 0;
    DWORD iAttr = 0;
    DWORD iValue = 0;
    DWORD iCount = 0;
    DWORD dwNumEntries = 0;
    DWORD dwNumMessages = 0;
    DWORD dwAttributeCount = 0;
    PDWORD pdwAttributes = NULL;
    LDAPMessage *pResNext = NULL;
    struct berval **berLdapRetQuery = NULL;

    if (!pLd || !pRes || !ppszAttributes || !ppdwAttributes)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    /* Total number of LDAP entries */
    dwNumEntries = ldap_count_entries(pLd, pRes);
    dwNumMessages = ldap_count_messages(pLd, pRes);
    if (dwNumEntries <= 0 || dwNumMessages <= 0)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    /* Count of attributes per search entry */
    dwError = LwAllocateMemory(sizeof(DWORD) * dwNumMessages,
                               (VOID*) &pdwAttributes);
    BAIL_ON_VMDIRDB_ERROR(dwError);

    pResNext = ldap_first_message(pLd, pRes);
    while (pResNext)
    {
        for (iAttr = 0, dwAttributeCount = 0; ppszAttributes[iAttr]; iAttr++)
        {
            berLdapRetQuery = ldap_get_values_len(pLd, pResNext, ppszAttributes[iAttr]);
            if (berLdapRetQuery)
            {
                iValue = ldap_count_values_len(berLdapRetQuery);
                if (iValue > 0)
                {
                    dwAttributeCount += iValue;
                }
            }
        }

        /* This is the total number of values per attribute */
        pdwAttributes[iCount] = dwAttributeCount;
        pResNext = ldap_next_message(pLd, pResNext);
        iCount++;
    }
    *ppdwAttributes = pdwAttributes;
    *pdwNumEntries = dwNumEntries;

cleanup:
    if (berLdapRetQuery)
    {
        ldap_value_free_len(berLdapRetQuery);
    }
    return dwError;

error:
    goto cleanup;
}

DWORD
VmdirDbAllocateEntriesAndAttributes(
    DWORD dwNumEntries,     /* Number of directory entries */
    PDWORD pdwAttributesCount,
    PDIRECTORY_ENTRY *ppDirectoryEntries)
{
    DWORD dwError = 0;
    DWORD i = 0;
    DWORD j = 0;
    PDIRECTORY_ENTRY pDirectoryEntries = NULL;
    PDIRECTORY_ATTRIBUTE pDirectoryAttributes = NULL;

    dwError = LwAllocateMemory(sizeof(DIRECTORY_ENTRY) * dwNumEntries,
                               (VOID*) &pDirectoryEntries);
    BAIL_ON_VMDIRDB_ERROR(dwError);

    for (i=0; i<dwNumEntries; i++)
    {
        dwError = LwAllocateMemory(sizeof(DIRECTORY_ATTRIBUTE) * pdwAttributesCount[i],
                               (VOID*) &pDirectoryAttributes);
        BAIL_ON_VMDIRDB_ERROR(dwError);

        for (j=0; j<pdwAttributesCount[i]; j++)
        {
            pDirectoryAttributes[j].ulNumValues = 1;
            dwError = LwAllocateMemory(sizeof(ATTRIBUTE_VALUE),
                                       (VOID*) &pDirectoryAttributes[j].pValues);
            BAIL_ON_VMDIRDB_ERROR(dwError);
        }

        pDirectoryEntries[i].ulNumAttributes = pdwAttributesCount[i];
        pDirectoryEntries[i].pAttributes = pDirectoryAttributes;
    }
    *ppDirectoryEntries = pDirectoryEntries;

cleanup:
    return dwError;

error:
    if (pDirectoryEntries)
    {
        for (i=0; i<dwNumEntries; i++)
        {
            LW_SAFE_FREE_MEMORY(pDirectoryEntries[i].pAttributes);
        }
    }
    goto cleanup;
}


/*
 * Walk through the LDAP responses and attributes, filling in the
 * allocated pDirectoryEntries structure ATTRIBUTE_VALUE entries
 */
static DWORD
VmdirDbAllocateEntriesAndAttributesValues(
    LDAP *pLd,
    LDAPMessage *pRes,
    PSTR *ppszAttributes,
    PDWORD pdwAttributeTypes,
    PDIRECTORY_ENTRY pDirectoryEntries)
{
    DWORD dwError = 0;
    LDAPMessage *pResNext = NULL;
    DWORD iEntry = 0;
    DWORD iAttr = 0;
    DWORD iAttrIndex = 0;
    DWORD i = 0;
    PWSTR pwszName = NULL;
    PWSTR pwszValue = NULL;
    NTSTATUS ntStatus = 0;
    struct berval **berLdapRetQuery = NULL;
    POCTET_STRING pBinaryData = NULL;

    if (!pLd || !pRes || !ppszAttributes || !pDirectoryEntries)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    pResNext = ldap_first_message(pLd, pRes);
    while (pResNext)
    {
        for (iAttr = 0; ppszAttributes[iAttr]; iAttr++)
        {
            berLdapRetQuery = ldap_get_values_len(pLd, pResNext, ppszAttributes[iAttr]);
            if (!berLdapRetQuery)
            {
                continue; /* for (iAttr) loop */
            }

            ntStatus = LwRtlWC16StringAllocateFromCString(
                           &pwszName,
                           ppszAttributes[iAttr]);
            BAIL_ON_VMDIRDB_ERROR(LwNtStatusToWin32Error(ntStatus));
            pDirectoryEntries[iEntry].pAttributes[iAttrIndex].pwszName = pwszName;

            for (i=0; berLdapRetQuery[i]; i++)
            {
                if (pdwAttributeTypes[iAttr] == DIRECTORY_ATTR_TYPE_UNICODE_STRING)
                {
                    ntStatus = LwRtlWC16StringAllocateFromCString(
                                   &pwszValue,
                                   berLdapRetQuery[i]->bv_val);
                    BAIL_ON_VMDIRDB_ERROR(LwNtStatusToWin32Error(ntStatus));

                    pDirectoryEntries[iEntry].pAttributes[iAttrIndex].pValues[i].data.pwszStringValue =
                        pwszValue;
                }
                else if (pdwAttributeTypes[iAttr] == DIRECTORY_ATTR_TYPE_NT_SECURITY_DESCRIPTOR)
                {
                    /* Deal with binary data types */
                    dwError = LwAllocateMemory(sizeof(OCTET_STRING),
                                               (VOID*) &pBinaryData);
                    BAIL_ON_VMDIRDB_ERROR(LwNtStatusToWin32Error(ntStatus));

                    dwError = LwAllocateMemory(
                                  sizeof(UCHAR) * berLdapRetQuery[i]->bv_len,
                                  (VOID*) &pBinaryData->pBytes);
                    BAIL_ON_VMDIRDB_ERROR(LwNtStatusToWin32Error(ntStatus));

                    memcpy(pBinaryData->pBytes,
                           berLdapRetQuery[i]->bv_val,
                           berLdapRetQuery[i]->bv_len);
                    pBinaryData->ulNumBytes = berLdapRetQuery[i]->bv_len;

                    pDirectoryEntries[iEntry].pAttributes[iAttrIndex].pValues[i].data.pOctetString =
                        pBinaryData;
                }

                /* DIRECTORY_ATTR_TYPE_UNICODE_STRING ~ PWSTR */
                pDirectoryEntries[iEntry].pAttributes[iAttrIndex].pValues[i].Type = pdwAttributeTypes[iAttr];
                iAttrIndex++;
            }
        }
        pResNext = ldap_next_message(pLd, pResNext);
        iEntry++;
    }

cleanup:
    return dwError;

error:
    goto cleanup;
}


DWORD
VmdirDbSearchObject(
    HANDLE            hDirectory,
    PWSTR             pwszBase,
    ULONG             ulScope,
    PWSTR             pwszFilter,
    PWSTR             wszAttributes[],
    ULONG             ulAttributesOnly,
    PDIRECTORY_ENTRY  *ppDirectoryEntries,
    PDWORD            pdwNumEntries
    )
{
    DWORD dwError = 0;
    NTSTATUS ntStatus = 0;
    PVMDIR_AUTH_PROVIDER_CONTEXT pContext = NULL;
    LDAP *pLd = NULL;
    int ldap_err = 0;
    PSTR pszLdapBase = NULL;
    PSTR pszBaseAlloc = NULL;
    PSTR pszFilter = NULL;
    PSTR pszLdapFilter = NULL;
    DWORD uLdapScope = 0;
    PSTR *ppszLdapAttributes = NULL;
    PSTR *ppszLdapAttributesAlloc = NULL;
    LDAPMessage *pRes = NULL;
    PDIRECTORY_ENTRY pDirectoryEntries = NULL;
    PDIRECTORY_ENTRY pTransformDirectoryEntries = NULL;
                      
    DWORD dwNumEntries = 0;
    PVMDIRDB_LDAPQUERY_MAP_ENTRY pQueryMapEntry = NULL;
    VMDIRDB_LDAPQUERY_MAP_ENTRY_TRANSFORM_FUNC pfnTransform = NULL;
    PDWORD pdwAttributesCount = NULL;
    PDWORD pdwLdapAttributeTypes = NULL;

    if (!hDirectory || !wszAttributes || !ppDirectoryEntries || !pdwNumEntries)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    pContext = (PVMDIR_AUTH_PROVIDER_CONTEXT) hDirectory;
    pLd = pContext->dirContext.pLd;

    if (pwszFilter)
    {
        ntStatus = LwRtlCStringAllocateFromWC16String(&pszFilter, pwszFilter);
        if (ntStatus)
        {
            dwError =  LwNtStatusToWin32Error(ntStatus);
            BAIL_ON_VMDIRDB_ERROR(dwError);
        }
    }


    dwError = VmDirFindLdapQueryMapEntry(
                  pszFilter,
                  &pQueryMapEntry);
    BAIL_ON_VMDIRDB_ERROR(dwError);
    pszLdapBase = pQueryMapEntry->pszLdapBase;
    pszLdapFilter = pQueryMapEntry->pszLdapQuery;
    uLdapScope = pQueryMapEntry->uScope;
    ppszLdapAttributes = pQueryMapEntry->ppszLdapAttributes;
    pfnTransform = pQueryMapEntry->pfnTransform;

    if (!ppszLdapAttributes)
    {

        /* Map SQL wszAttributes to LDAP attributes array */
        dwError = VmdirFindLdapPwszAttributeList(
                      wszAttributes,
                      &ppszLdapAttributesAlloc,
                      &pdwLdapAttributeTypes);
        BAIL_ON_VMDIRDB_ERROR(dwError);
        ppszLdapAttributes = ppszLdapAttributesAlloc;
    }
#if 1 /* TBD:Adam-"entryDn" search failure in this case is causing problems */
    else
    {
        /* Map overrite ppszLdapAttributes to LDAP attributes array */
        dwError = VmdirFindLdapAttributeList(
                      ppszLdapAttributes,
                      &ppszLdapAttributesAlloc,
                      &pdwLdapAttributeTypes);
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }
#endif

    ldap_err = ldap_search_ext_s(pLd,
                                 pszLdapBase,
                                 uLdapScope,
                                 pszLdapFilter,
                                 ppszLdapAttributes,
                                 ulAttributesOnly,
                                 NULL, /* serverctrls */
                                 NULL, /* serverctrls */
                                 NULL, /* timeout */
                                 0,    /* sizelimit */
                                 &pRes);
    if (ldap_err)
    {
        dwError = LwMapLdapErrorToLwError(ldap_err);
        BAIL_ON_VMDIRDB_ERROR(dwError);
    }

    /* TBD:Adam-Loop through each entry and allocate the number of attribute structures */
    dwError = VmdirDbCountEntriesAndAttributes(
                  pLd,
                  pRes,
                  ppszLdapAttributes,
                  &dwNumEntries,   /* Number of directory entries */
                  &pdwAttributesCount);
    BAIL_ON_VMDIRDB_ERROR(dwError);

    dwError = VmdirDbAllocateEntriesAndAttributes(
                  dwNumEntries,
                  pdwAttributesCount,
                  &pDirectoryEntries);
    BAIL_ON_VMDIRDB_ERROR(dwError);

    /* Populate attribute values for DIRECTORY_ENTRIES */
    dwError = VmdirDbAllocateEntriesAndAttributesValues(
                  pLd,
                  pRes,
                  ppszLdapAttributes,
                  pdwLdapAttributeTypes,
                  pDirectoryEntries);
    BAIL_ON_VMDIRDB_ERROR(dwError);

    if (pfnTransform)
    {
#if 1
        dwError = pfnTransform(
                      dwNumEntries,
                      pDirectoryEntries,
                      &pTransformDirectoryEntries);
        BAIL_ON_VMDIRDB_ERROR(dwError);

        /* TBD:Adam-Cleanup the entire pDirectoryEntries structure on some failure */
        LW_SAFE_FREE_MEMORY(pDirectoryEntries);
        pDirectoryEntries = pTransformDirectoryEntries;
#else
        dwError = VmdirDbAllocateEntriesAndAttributes(
                      dwNumEntries,
                      pdwAttributesCount,
                      &pTransformDirectoryEntries);
        BAIL_ON_VMDIRDB_ERROR(dwError);

        /* TBD:Adam-Cleanup the entire pDirectoryEntries structure on some failure */
        LW_SAFE_FREE_MEMORY(pDirectoryEntries);
        pDirectoryEntries = pTransformDirectoryEntries;
#endif

    }

    *ppDirectoryEntries = pDirectoryEntries;
    *pdwNumEntries = dwNumEntries;

cleanup:
    VmDirAttributesFree(&ppszLdapAttributesAlloc);
    LW_SAFE_FREE_STRING(pszBaseAlloc);
    LW_SAFE_FREE_STRING(pszFilter);
    return dwError;

error:
    /* TBD:Adam-Cleanup the entire pDirectoryEntries structure on some failure */
    LW_SAFE_FREE_MEMORY(pDirectoryEntries);
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
