/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

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
 *        lsahash.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS) Hashtable
 *
 * Authors: Kyle Stemen (kstemen@likewisesoftware.com)
 *
 */
#include "includes.h"

DWORD
LsaHashCreate(
        size_t sTableSize,
        LSA_HASH_KEY_COMPARE fnComparator,
        LSA_HASH_KEY fnHash,
        LSA_HASH_FREE_ENTRY fnFree, //optional
        LSA_HASH_COPY_ENTRY fnCopy, //optional
        LSA_HASH_TABLE** ppResult)
{
    LSA_HASH_TABLE *pResult = NULL;
    DWORD dwError = LW_ERROR_SUCCESS;

    dwError = LsaAllocateMemory(
                    sizeof(*pResult),
                    (PVOID*)&pResult);
    BAIL_ON_LSA_ERROR(dwError);

    pResult->sTableSize = sTableSize;
    pResult->sCount = 0;
    pResult->fnComparator = fnComparator;
    pResult->fnHash = fnHash;
    pResult->fnFree = fnFree;
    pResult->fnCopy = fnCopy;

    dwError = LsaAllocateMemory(
                    sizeof(*pResult->ppEntries) * sTableSize,
                    (PVOID*)&pResult->ppEntries);
    BAIL_ON_LSA_ERROR(dwError);

    *ppResult = pResult;

cleanup:
    return dwError;

error:
    LsaHashSafeFree(&pResult);
    goto cleanup;
}

//Don't call this
void
LsaHashFree(
        LSA_HASH_TABLE* pResult)
{
    LsaHashSafeFree(&pResult);
}

void
LsaHashSafeFree(
        LSA_HASH_TABLE** ppResult)
{
    DWORD dwError = LW_ERROR_SUCCESS;
    LSA_HASH_ITERATOR iterator;
    LSA_HASH_ENTRY *pEntry = NULL;

    if(*ppResult == NULL)
    {
        goto cleanup;
    }

    dwError = LsaHashGetIterator(*ppResult, &iterator);
    BAIL_ON_LSA_ERROR(dwError);

    while ((pEntry = LsaHashNext(&iterator)) != NULL)
    {
        if ((*ppResult)->fnFree != NULL)
        {
            (*ppResult)->fnFree(pEntry);
        }
        LSA_SAFE_FREE_MEMORY(pEntry);
    }

    LSA_SAFE_FREE_MEMORY((*ppResult)->ppEntries);
    LsaFreeMemory(*ppResult);

    *ppResult = NULL;

cleanup:
error:
    ;
}

DWORD
LsaHashSetValue(
        LSA_HASH_TABLE *pTable,
        PVOID  pKey,
        PVOID  pValue)
{
    DWORD dwError = LW_ERROR_SUCCESS;
    size_t sBucket = pTable->fnHash(pKey) % pTable->sTableSize;
    LSA_HASH_ENTRY **ppExamine = &pTable->ppEntries[sBucket];
    LSA_HASH_ENTRY *pNewEntry = NULL;

    while (*ppExamine != NULL)
    {
        //There's a hash collision
        if (!pTable->fnComparator((*ppExamine)->pKey, pKey))
        {
            //The key already exists; replace it
            if (pTable->fnFree != NULL)
            {
                pTable->fnFree(*ppExamine);
            }

            (*ppExamine)->pKey = pKey;
            (*ppExamine)->pValue = pValue;
            goto cleanup;
        }

        ppExamine = &(*ppExamine)->pNext;
    }

    //The key isn't in the table yet.
    dwError = LsaAllocateMemory(
                    sizeof(*pNewEntry),
                    (PVOID*)&pNewEntry);
    BAIL_ON_LSA_ERROR(dwError);
    pNewEntry->pKey = pKey;
    pNewEntry->pValue = pValue;

    *ppExamine = pNewEntry;
    pTable->sCount++;

cleanup:
    return dwError;

error:
    LSA_SAFE_FREE_MEMORY(pNewEntry);
    goto cleanup;
}

//Returns ENOENT if pKey is not in the table
DWORD
LsaHashGetValue(
        LSA_HASH_TABLE *pTable,
        PCVOID  pKey,
        PVOID* ppValue)
{
    DWORD dwError = LW_ERROR_SUCCESS;
    size_t sBucket = 0;
    LSA_HASH_ENTRY *pExamine = NULL;

    if (pTable->sTableSize > 0)
    {
        sBucket = pTable->fnHash(pKey) % pTable->sTableSize;
        pExamine = pTable->ppEntries[sBucket];
    }

    while (pExamine != NULL)
    {
        if (!pTable->fnComparator(pExamine->pKey, pKey))
        {
            //Found it!
            if (ppValue != NULL)
            {
                *ppValue = pExamine->pValue;
            }
            goto cleanup;
        }

        pExamine = pExamine->pNext;
    }

    dwError = ENOENT;

cleanup:

    return dwError;
}

BOOLEAN
LsaHashExists(
    IN PLSA_HASH_TABLE pTable,
    IN PCVOID pKey
    )
{
    DWORD dwError = LsaHashGetValue(pTable, pKey, NULL);
    return (LW_ERROR_SUCCESS == dwError) ? TRUE : FALSE;
}

DWORD
LsaHashCopy(
    IN  LSA_HASH_TABLE *pTable,
    OUT LSA_HASH_TABLE **ppResult
    )
{
    DWORD             dwError = LW_ERROR_SUCCESS;
    LSA_HASH_ITERATOR iterator;
    LSA_HASH_ENTRY    EntryCopy;
    LSA_HASH_ENTRY    *pEntry = NULL;
    LSA_HASH_TABLE    *pResult = NULL;

    memset(&EntryCopy, 0, sizeof(EntryCopy));

    dwError = LsaHashCreate(
                  pTable->sTableSize,
                  pTable->fnComparator,
                  pTable->fnHash,
                  pTable->fnCopy ? pTable->fnFree : NULL,
                  pTable->fnCopy,
                  &pResult);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaHashGetIterator(pTable, &iterator);
    BAIL_ON_LSA_ERROR(dwError);

    while ((pEntry = LsaHashNext(&iterator)) != NULL)
    {
        if ( pTable->fnCopy )
        {
            dwError = pTable->fnCopy(pEntry, &EntryCopy);
            BAIL_ON_LSA_ERROR(dwError);
        }
        else
        {
            EntryCopy.pKey = pEntry->pKey;
            EntryCopy.pValue = pEntry->pValue;
        }

        dwError = LsaHashSetValue(
                      pResult,
                      EntryCopy.pKey,
                      EntryCopy.pValue);
        BAIL_ON_LSA_ERROR(dwError);

        memset(&EntryCopy, 0, sizeof(EntryCopy));
    }

    *ppResult = pResult;

cleanup:

    return dwError;

error:

    if ( pTable->fnCopy && pTable->fnFree )
    {
        pTable->fnFree(&EntryCopy);
    }

    LsaHashSafeFree(&pResult);

    goto cleanup;
}

//Invalidates all iterators
DWORD
LsaHashResize(
        LSA_HASH_TABLE *pTable,
        size_t sTableSize)
{
    DWORD dwError = LW_ERROR_SUCCESS;
    LSA_HASH_ENTRY **ppEntries;
    LSA_HASH_ITERATOR iterator;
    LSA_HASH_ENTRY *pEntry = NULL;
    size_t sBucket;

    dwError = LsaAllocateMemory(
                    sizeof(*ppEntries) * sTableSize,
                    (PVOID*)&ppEntries);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaHashGetIterator(pTable, &iterator);
    BAIL_ON_LSA_ERROR(dwError);

    while ((pEntry = LsaHashNext(&iterator)) != NULL)
    {
        sBucket = pTable->fnHash(pEntry->pKey) % sTableSize;
        pEntry->pNext = ppEntries[sBucket];
        ppEntries[sBucket] = pEntry;
    }

    LSA_SAFE_FREE_MEMORY(pTable->ppEntries);
    pTable->ppEntries = ppEntries;
    pTable->sTableSize = sTableSize;

cleanup:
    return dwError;

error:
    LSA_SAFE_FREE_MEMORY(ppEntries);

    goto cleanup;
}

DWORD
LsaHashGetIterator(
        LSA_HASH_TABLE *pTable,
        LSA_HASH_ITERATOR *pIterator)
{
    pIterator->pTable = pTable;
    pIterator->sEntryIndex = 0;
    if(pTable->sTableSize > 0)
    {
        pIterator->pEntryPos = pTable->ppEntries[0];
    }
    else
    {
        pIterator->pEntryPos = NULL;
    }

    return LW_ERROR_SUCCESS;
}

// returns NULL after passing the last entry
LSA_HASH_ENTRY *
LsaHashNext(
        LSA_HASH_ITERATOR *pIterator
        )
{
    LSA_HASH_ENTRY *pRet;

    // If there are any entries left, return a non-null entry
    while (pIterator->pEntryPos == NULL &&
            pIterator->sEntryIndex < pIterator->pTable->sTableSize)
    {
        pIterator->sEntryIndex++;
        if (pIterator->sEntryIndex < pIterator->pTable->sTableSize)
        {
            pIterator->pEntryPos = pIterator->pTable->ppEntries[
                pIterator->sEntryIndex];
        }
    }

    pRet = pIterator->pEntryPos;
    if (pRet != NULL)
    {
        //Advance the iterator
        pIterator->pEntryPos = pRet->pNext;
    }

    return pRet;
}

DWORD
LsaHashRemoveKey(
        LSA_HASH_TABLE *pTable,
        PVOID  pKey)
{
    DWORD dwError = LW_ERROR_SUCCESS;
    size_t sBucket = pTable->fnHash(pKey) % pTable->sTableSize;
    LSA_HASH_ENTRY **ppExamine = &pTable->ppEntries[sBucket];
    LSA_HASH_ENTRY *pDelete;

    while (*ppExamine != NULL)
    {
        if (!pTable->fnComparator((*ppExamine)->pKey, pKey))
        {
            //Found it!
            pDelete = *ppExamine;
            if (pTable->fnFree != NULL)
            {
                pTable->fnFree(pDelete);
            }

            //Remove it from the list
            pTable->sCount--;
            *ppExamine = pDelete->pNext;
            LSA_SAFE_FREE_MEMORY(pDelete);
            goto cleanup;
        }

        ppExamine = &(*ppExamine)->pNext;
    }

    //The key isn't in the table yet.
    dwError = ENOENT;

cleanup:
    return dwError;
}

int
LsaHashCaselessStringCompare(
        PCVOID str1,
        PCVOID str2)
{
    return strcasecmp((PCSTR)str1, (PCSTR)str2);
}

size_t
LsaHashCaselessString(
        PCVOID str)
{
    size_t result = 0;
    PCSTR pos = (PCSTR)str;
    int lowerChar;

    while (*pos != '\0')
    {
        // rotate result to the left 3 bits with wrap around
        result = (result << 3) | (result >> (sizeof(size_t)*8 - 3));

        lowerChar = tolower(*pos);
        result ^= lowerChar;
        result += lowerChar;
        pos++;
    }

    return result;
}

VOID
LsaHashFreeStringKey(
    IN OUT const LSA_HASH_ENTRY *pEntry
    )
{
    if (pEntry->pKey)
    {
        LsaFreeString(pEntry->pKey);
    }
}

/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
