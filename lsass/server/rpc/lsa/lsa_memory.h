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
 *        lsa_memory.h
 *
 * Abstract:
 *
 *        Remote Procedure Call (RPC) Server Interface
 *
 *        Lsa memory allocation manager
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#ifndef _LSASRV_MEMORY_H_
#define _LSASRV_MEMORY_H_


NTSTATUS
LsaSrvInitMemory(
    void
    );


NTSTATUS
LsaSrvDestroyMemory(
    void
    );


NTSTATUS
LsaSrvAllocateMemory(
    void **ppOut,
    DWORD dwSize,
    void *pDep
    );


NTSTATUS
LsaSrvReallocMemory(
    void **ppOut,
    DWORD dwNewSize,
    void *pIn
    );


NTSTATUS
LsaSrvAddDepMemory(
    void *pIn,
    void *pDep
    );


void
LsaSrvFreeMemory(
    void *pPtr
    );


NTSTATUS
LsaSrvAllocateSidFromWC16String(
    PSID *ppSid,
    PCWSTR pwszSidStr,
    void *pParent
    );


NTSTATUS
LsaSrvDuplicateSid(
    PSID *ppSidOut,
    PSID pSidIn,
    void *pParent
    );


NTSTATUS
LsaSrvDuplicateWC16String(
    PWSTR *ppwszOut,
    PWSTR pwszIn,
    void *pParent
    );


NTSTATUS
LsaSrvDuplicateWC16String(
    PWSTR *ppwszOut,
    PWSTR pwszIn,
    void *pParent
    );


NTSTATUS
LsaSrvGetFromUnicodeString(
    PWSTR *ppwszOut,
    UnicodeString *pIn,
    void *pParent
    );


NTSTATUS
LsaSrvGetFromUnicodeStringEx(
    PWSTR *ppwszOut,
    UnicodeStringEx *pIn,
    void *pParent
    );


NTSTATUS
LsaSrvInitUnicodeString(
    UnicodeString *pOut,
    PCWSTR pwszIn,
    void *pParent
    );


NTSTATUS
LsaSrvInitUnicodeStringEx(
    UnicodeStringEx *pOut,
    PCWSTR pwszIn,
    void *pParent
    );


NTSTATUS
LsaSrvDuplicateUnicodeStringEx(
    UnicodeStringEx *pOut,
    UnicodeStringEx *pIn,
    void *pParent
    );


NTSTATUS
LsaSrvSidAppendRid(
    PSID *ppOutSid,
    PSID pInSid,
    DWORD dwRid,
    void *pParent
    );


#endif /* _LSASRV_MEMORY_H_ */
