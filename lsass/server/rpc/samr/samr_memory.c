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
 *        samr_memeory.c
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        Samr memory allocation manager
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#include "includes.h"


NTSTATUS
SamrSrvAllocateMemory(
    void **ppOut,
    DWORD dwSize
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pOut = NULL;

    pOut = rpc_ss_allocate(dwSize);
    BAIL_ON_NO_MEMORY(pOut);

    memset(pOut, 0, dwSize);

    *ppOut = pOut;

cleanup:
    return ntStatus;

error:
    *ppOut = NULL;
    goto cleanup;
}


void
SamrSrvFreeMemory(
    void *pPtr
    )
{
    rpc_ss_free(pPtr);
}


NTSTATUS
SamrSrvAllocateSidFromWC16String(
    PSID *ppSid,
    PCWSTR pwszSidStr
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PSID pSid = NULL;
    ULONG ulSidSize = 0;
    PSID pSidCopy = NULL;

    ntStatus = RtlAllocateSidFromWC16String(&pSid,
                                          pwszSidStr);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    ulSidSize = RtlLengthSid(pSid);
    ntStatus = SamrSrvAllocateMemory((void**)&pSidCopy,
                                   ulSidSize);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    ntStatus = RtlCopySid(ulSidSize, pSidCopy, pSid);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    *ppSid = pSidCopy;

cleanup:
    if (pSid) {
        RTL_FREE(&pSid);
    }

    return ntStatus;

error:
    if (pSidCopy) {
        SamrSrvFreeMemory(pSidCopy);
    }

    *ppSid = NULL;
    goto cleanup;
}


NTSTATUS
SamrSrvDuplicateSid(
    PSID *ppSidOut,
    PSID pSidIn
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PSID pSid = NULL;
    ULONG ulSidSize = 0;

    ulSidSize = RtlLengthSid(pSidIn);
    ntStatus = SamrSrvAllocateMemory((void**)&pSid,
                                   ulSidSize);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    ntStatus = RtlCopySid(ulSidSize, pSid, pSidIn);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    *ppSidOut = pSid;

cleanup:
    return ntStatus;

error:
    if (pSid) {
        SamrSrvFreeMemory(pSid);
    }

    *ppSidOut = NULL;
    goto cleanup;
}


NTSTATUS
SamrSrvGetFromUnicodeString(
    PWSTR *ppwszOut,
    UnicodeString *pIn
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PWSTR pwszStr = NULL;

    ntStatus = SamrSrvAllocateMemory((void**)&pwszStr,
                                   (pIn->size + 1) * sizeof(WCHAR));
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    wc16sncpy(pwszStr, pIn->string, (pIn->len / sizeof(WCHAR)));
    *ppwszOut = pwszStr;

cleanup:
    return ntStatus;

error:
    if (pwszStr) {
        SamrSrvFreeMemory(pwszStr);
    }

    *ppwszOut = NULL;
    goto cleanup;
}


NTSTATUS
SamrSrvGetFromUnicodeStringEx(
    PWSTR *ppwszOut,
    UnicodeStringEx *pIn
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PWSTR pwszStr = NULL;

    ntStatus = SamrSrvAllocateMemory((void**)&pwszStr,
                                   (pIn->size) * sizeof(WCHAR));
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    wc16sncpy(pwszStr, pIn->string, (pIn->len / sizeof(WCHAR)));
    *ppwszOut = pwszStr;

cleanup:
    return ntStatus;

error:
    if (pwszStr) {
        SamrSrvFreeMemory(pwszStr);
    }

    *ppwszOut = NULL;
    goto cleanup;
}


NTSTATUS
SamrSrvInitUnicodeString(
    UnicodeString *pOut,
    PCWSTR pwszIn
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DWORD dwLen = 0;
    DWORD dwSize = 0;

    dwLen  = (pwszIn) ? wc16slen(pwszIn) : 0;
    dwSize = dwLen * sizeof(WCHAR);

    ntStatus = SamrSrvAllocateMemory((void**)&(pOut->string),
                                   dwSize);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    memcpy(pOut->string, pwszIn, dwSize);
    pOut->size = dwSize;
    pOut->len  = dwSize;

cleanup:
    return ntStatus;

error:
    if (pOut->string) {
        SamrSrvFreeMemory(pOut->string);
    }

    pOut->size = 0;
    pOut->len  = 0;
    goto cleanup;
}


NTSTATUS
SamrSrvInitUnicodeStringEx(
    UnicodeStringEx *pOut,
    PCWSTR pwszIn
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DWORD dwLen = 0;
    DWORD dwSize = 0;

    dwLen  = (pwszIn) ? wc16slen(pwszIn) : 0;
    dwSize = (dwLen + 1) * sizeof(WCHAR);

    ntStatus = SamrSrvAllocateMemory((void**)&(pOut->string),
                                   dwSize);
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    memcpy(pOut->string, pwszIn, dwSize - 1);
    pOut->size = dwSize;
    pOut->len  = dwSize - 1;

cleanup:
    return ntStatus;

error:
    if (pOut->string) {
        SamrSrvFreeMemory(pOut->string);
    }

    pOut->size = 0;
    pOut->len  = 0;
    goto cleanup;
}


void
SamrSrvFreeUnicodeString(
    UnicodeString *pStr
    )
{
    if (pStr->string)
    {
        SamrSrvFreeMemory(pStr->string);
    }
    pStr->len  = 0;
    pStr->size = 0;
}


void
SamrSrvFreeUnicodeStringEx(
    UnicodeStringEx *pStr
    )
{
    if (pStr->string)
    {
        SamrSrvFreeMemory(pStr->string);
    }
    pStr->len  = 0;
    pStr->size = 0;
}


NTSTATUS
SamrSrvAllocateSecDescBuffer(
    PSECURITY_DESCRIPTOR_BUFFER *ppBuffer,
    POCTET_STRING                pBlob
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PSECURITY_DESCRIPTOR_BUFFER pBuffer = NULL;

    ntStatus = SamrSrvAllocateMemory((void**)&pBuffer,
                                     sizeof(*pBuffer));
    BAIL_ON_NTSTATUS_ERROR(ntStatus);

    if (pBlob && pBlob->ulNumBytes)
    {
        pBuffer->ulBufferLen = pBlob->ulNumBytes;

        ntStatus = SamrSrvAllocateMemory((void**)&pBuffer->pBuffer,
                                         pBlob->ulNumBytes);
        BAIL_ON_NTSTATUS_ERROR(ntStatus);

        memcpy(pBuffer->pBuffer,
               pBlob->pBytes,
               pBuffer->ulBufferLen);
    }

    *ppBuffer = pBuffer;

cleanup:
    return ntStatus;

error:
    if (pBuffer)
    {
        if (pBuffer->pBuffer)
        {
            SamrSrvFreeMemory(pBuffer->pBuffer);
        }

        SamrSrvFreeMemory(pBuffer);
    }

    *ppBuffer = NULL;
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
