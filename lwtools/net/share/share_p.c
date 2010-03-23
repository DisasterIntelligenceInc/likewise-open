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
 *        share_p.c
 *
 * Abstract:
 *
 *        Likewise System NET Utilities
 *
 *        Share Helper Routines
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Wei Fu (wfu@likewise.com)
 */

#include "includes.h"

NET_SHARE_STATE gState = {0};

static
DWORD
MapNameToSid(
    HANDLE hLsa,
    PCWSTR pwszName,
    PSID* ppSid
    );

static
DWORD
MapBuiltinNameToSid(
    PSID *ppSid,
    PCWSTR pwszName
    );

static
DWORD
ConstructSecurityDescriptor(
    DWORD dwAllowUserCount,
    PWSTR* ppwszAllowUsers,
    DWORD dwDenyUserCount,
    PWSTR* ppwszDenyUsers,
    BOOLEAN bReadOnly,
    PSECURITY_DESCRIPTOR_RELATIVE* ppRelative,
    PDWORD pdwRelativeSize
    );



DWORD
LwUtilNetShareAdd(
	NET_SHARE_ADD_INFO_PARAMS ShareAddInfo
    )
{
    static const DWORD dwLevel = 502;

    DWORD dwError = 0;
    SHARE_INFO_502 shareInfo = {0};
    DWORD dwParmErr = 0;
    PSECURITY_DESCRIPTOR_RELATIVE pSecDesc = NULL;
    DWORD dwSecDescSize = 0;


    dwError = LwMbsToWc16s(ShareAddInfo.pszPath, &gState.pwszPath);
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwMbsToWc16s(ShareAddInfo.pszShareName, &gState.pwszName);
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = ConstructSecurityDescriptor(
        gState.dwAllowUserCount,
        gState.ppwszAllowUsers,
        gState.dwDenyUserCount,
        gState.ppwszDenyUsers,
        gState.bReadOnly && !gState.bReadWrite,
        &pSecDesc,
        &dwSecDescSize);
    BAIL_ON_LWUTIL_ERROR(dwError);

    shareInfo.shi502_type = 0; // SHARE_SERVICE_DISK_SHARE
    shareInfo.shi502_netname = gState.pwszName ? gState.pwszName : gState.pwszTarget;
    shareInfo.shi502_remark = gState.pwszComment;
    shareInfo.shi502_path = gState.pwszPath;
    shareInfo.shi502_reserved = dwSecDescSize;
    shareInfo.shi502_security_descriptor = (PBYTE) pSecDesc;

    dwError = NetShareAddW(
        gState.pwszServerName,
        dwLevel,
        (PBYTE)&shareInfo,
        &dwParmErr);
    BAIL_ON_LWUTIL_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_MEMORY(pSecDesc);

    return dwError;

error:

    goto cleanup;
}


DWORD
LwUtilNetShareEnum(
    VOID
    )
{
    static const DWORD dwLevel = 2;
    static const DWORD dwMaxLen = 128;

    DWORD dwError = 0;
    PSHARE_INFO_2 pShareInfo = NULL;
    DWORD dwNumShares = 0;
    DWORD dwTotalShares = 0;
    DWORD dwVisitedShares = 0;
    DWORD dwResume = 0;
    DWORD dwIndex = 0;

    PSTR* ppszShareName = NULL;
    PSTR* ppszSharePath = NULL;
    PSTR* ppszShareComment = NULL;
    DWORD dwShareNameLenMax = 0;
    DWORD dwSharePathLenMax = 0;
    DWORD dwShareCommentLenMax = 0;
    DWORD dwShareNameLen = 0;
    DWORD dwSharePathLen = 0;
    DWORD dwShareCommentLen = 0;


    do
    {
        dwError = NetShareEnumW(
            gState.pwszServerName,
            dwLevel,
            (PBYTE*)&pShareInfo,
            dwMaxLen,
            &dwNumShares,
            &dwTotalShares,
            &dwResume);
        BAIL_ON_LWUTIL_ERROR(dwError);

        if (!ppszShareName)
        {
            dwError = LwAllocateMemory((dwTotalShares+1)*sizeof(PCSTR), (PVOID *)&ppszShareName);
            BAIL_ON_LWUTIL_ERROR(dwError);
        }

        if (!ppszSharePath)
        {
            dwError = LwAllocateMemory((dwTotalShares+1)*sizeof(PCSTR), (PVOID *)&ppszSharePath);
            BAIL_ON_LWUTIL_ERROR(dwError);
        }

        if (!ppszShareComment)
        {
        	dwError = LwAllocateMemory((dwTotalShares+1)*sizeof(PCSTR), (PVOID *)&ppszShareComment);
            BAIL_ON_LWUTIL_ERROR(dwError);
        }

        for (dwIndex = 0; dwIndex < dwNumShares; dwIndex++)
        {
            dwError = LwWc16sToMbs(pShareInfo[dwIndex].shi2_netname,
            		               &ppszShareName[dwIndex+dwVisitedShares]);
            BAIL_ON_LWUTIL_ERROR(dwError);

            dwError = LwWc16sToMbs(pShareInfo[dwIndex].shi2_path,
            		               &ppszSharePath[dwIndex+dwVisitedShares]);
            BAIL_ON_LWUTIL_ERROR(dwError);

            if (pShareInfo[dwIndex].shi2_remark)
            {
                dwError = LwWc16sToMbs(pShareInfo[dwIndex].shi2_remark,
                		               &ppszShareComment[dwIndex+dwVisitedShares]);
                BAIL_ON_LWUTIL_ERROR(dwError);
            }
        }

        if (pShareInfo)
        {
            NetApiBufferFree(pShareInfo);
            pShareInfo = NULL;
        }

        dwVisitedShares += dwNumShares;

    } while (dwVisitedShares < dwTotalShares);

    dwError = LwAllocateString(NET_SHARE_NAME_TITLE, &ppszShareName[dwTotalShares]);
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwAllocateString(NET_SHARE_PATH_TITLE, &ppszSharePath[dwTotalShares]);
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwAllocateString(NET_SHARE_COMMENT_TITLE, &ppszShareComment[dwTotalShares]);
    BAIL_ON_LWUTIL_ERROR(dwError);

    for (dwIndex = 0; dwIndex < dwTotalShares + 1; dwIndex++)
    {
        dwShareNameLen = strlen(ppszShareName[dwIndex]);
        if (dwShareNameLen>dwShareNameLenMax)
        {
        	dwShareNameLenMax = dwShareNameLen;
        }

        dwSharePathLen = strlen(ppszSharePath[dwIndex]);
        if (dwSharePathLen>dwSharePathLenMax)
        {
        	dwSharePathLenMax = dwSharePathLen;
        }

        if (ppszShareComment[dwIndex])
        {
            dwShareCommentLen = strlen(ppszShareComment[dwIndex]);
            if (dwShareCommentLen>dwShareCommentLenMax)
            {
            	dwShareCommentLenMax = dwShareCommentLen;
            }
        }
    }

    //print share enum header

    printf("  %s%*s",
    		NET_SHARE_NAME_TITLE,
           (int) (strlen(NET_SHARE_NAME_TITLE)-dwShareNameLenMax),
           "");
    printf("  %s%*s",
    		NET_SHARE_PATH_TITLE,
           (int) (strlen(NET_SHARE_PATH_TITLE)-dwSharePathLenMax),
           "");
    printf("  %s%*s\n",
    		NET_SHARE_COMMENT_TITLE,
           (int) (strlen(NET_SHARE_COMMENT_TITLE)-dwShareCommentLenMax),
           "");

    for (dwIndex = 0; dwIndex < dwShareNameLenMax+dwSharePathLenMax+dwShareCommentLenMax+10; dwIndex++)
    	printf("%s", "-");

    printf("\n");


    for (dwIndex = 0; dwIndex < dwTotalShares; dwIndex++)
    {
        printf("  %s%*s",
        		ppszShareName[dwIndex],
               (int) (strlen(ppszShareName[dwIndex])-dwShareNameLenMax),
               "");

        printf("  %s%*s",
        		ppszSharePath[dwIndex],
               (int) (strlen(ppszSharePath[dwIndex])-dwSharePathLenMax),
               "");

        if (ppszShareComment[dwIndex])
        {
        	printf("  %s%*s",
        		ppszShareComment[dwIndex],
               (int) (strlen(ppszShareComment[dwIndex])-dwShareCommentLenMax),
               "");
        }

        printf("\n");
    }

cleanup:

    if (ppszShareName)
    {
    	LwFreeStringArray(ppszShareName, dwTotalShares);
    }
    if (ppszSharePath)
    {
    	LwFreeStringArray(ppszSharePath, dwTotalShares);
    }
    if (ppszShareComment)
    {
    	LwFreeStringArray(ppszShareComment, dwTotalShares);
    }

    if (pShareInfo)
    {
    	NetApiBufferFree(pShareInfo);
        pShareInfo = NULL;
    }

    return dwError;

error:

    goto cleanup;
}

DWORD
LwUtilNetShareDel(
	NET_SHARE_DEL_INFO_PARAMS ShareDelInfo
    )
{
    DWORD dwError = 0;
    PWSTR pwszShareName = NULL;

    dwError = LwMbsToWc16s(ShareDelInfo.pszShareName, &pwszShareName);
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = NetShareDelW(
        gState.pwszServerName,
        pwszShareName,
        0);
    BAIL_ON_LWUTIL_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_MEMORY(pwszShareName);

    return dwError;

error:

    goto cleanup;
}

static
DWORD
MapNameToSid(
    HANDLE hLsa,
    PCWSTR pwszName,
    PSID* ppSid
    )
{
    DWORD dwError = 0;
    PSTR pszName = NULL;
    LSA_QUERY_LIST QueryList;
    PLSA_SECURITY_OBJECT* ppObjects = NULL;

    dwError = LwWc16sToMbs(pwszName, &pszName);
    BAIL_ON_LWUTIL_ERROR(dwError);

    QueryList.ppszStrings = (PCSTR*) &pszName;

    dwError = LsaFindObjects(
        hLsa,
        NULL,
        0,
        LSA_OBJECT_TYPE_UNDEFINED,
        LSA_QUERY_TYPE_BY_NAME,
        1,
        QueryList,
        &ppObjects);
    BAIL_ON_LWUTIL_ERROR(dwError);

    if (ppObjects[0] == NULL)
    {
        dwError = LW_ERROR_NO_SUCH_OBJECT;
        BAIL_ON_LWUTIL_ERROR(dwError);
    }

    dwError = LwNtStatusToWin32Error(
        RtlAllocateSidFromCString(ppSid, ppObjects[0]->pszObjectSid));
    BAIL_ON_LWUTIL_ERROR(dwError);

cleanup:

    LsaFreeSecurityObjectList(1, ppObjects);

    LW_SAFE_FREE_STRING(pszName);

    return dwError;

error:

    *ppSid = NULL;

    goto cleanup;
}

static
DWORD
MapBuiltinNameToSid(
    PSID *ppSid,
    PCWSTR pwszName
    )
{
    DWORD dwError = 0;
    union
    {
        SID sid;
        BYTE buffer[SID_MAX_SIZE];
    } Sid;
    ULONG SidSize = sizeof(Sid.buffer);
    PWSTR pwszEveryone = NULL;

    dwError = LwNtStatusToWin32Error(
                  RtlWC16StringAllocateFromCString(
                      &pwszEveryone,
                      "Everyone"));
    BAIL_ON_LWUTIL_ERROR(dwError);


    if (LwRtlWC16StringIsEqual(pwszName, pwszEveryone, FALSE))
    {
        dwError = LwNtStatusToWin32Error(
                      RtlCreateWellKnownSid(
                          WinWorldSid,
                          NULL,
                          &Sid.sid,
                          &SidSize));
    }
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
                  RtlDuplicateSid(ppSid, &Sid.sid));

cleanup:
    LwRtlWC16StringFree(&pwszEveryone);

    return dwError;

error:
    goto cleanup;
}

static
DWORD
ConstructSecurityDescriptor(
    DWORD dwAllowUserCount,
    PWSTR* ppwszAllowUsers,
    DWORD dwDenyUserCount,
    PWSTR* ppwszDenyUsers,
    BOOLEAN bReadOnly,
    PSECURITY_DESCRIPTOR_RELATIVE* ppRelative,
    PDWORD pdwRelativeSize
    )
{
    DWORD dwError = 0;
    PSECURITY_DESCRIPTOR_ABSOLUTE pAbsolute = NULL;
    PSECURITY_DESCRIPTOR_RELATIVE pRelative = NULL;
    union
    {
        SID sid;
        BYTE buffer[SID_MAX_SIZE];
    } Owner;
    union
    {
        SID sid;
        BYTE buffer[SID_MAX_SIZE];
    } Group;
    ULONG OwnerSidSize = sizeof(Owner.buffer);
    ULONG GroupSidSize = sizeof(Group.buffer);
    DWORD dwDaclSize = 0;
    PACL pDacl = NULL;
    DWORD dwIndex = 0;
    PSID pSid = NULL;
    ULONG ulRelativeSize = 0;
    HANDLE hLsa = NULL;
    ACCESS_MASK mask = bReadOnly ?
                       (FILE_GENERIC_READ|FILE_GENERIC_EXECUTE) :
                       FILE_ALL_ACCESS;

    dwError = LsaOpenServer(&hLsa);
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlCreateWellKnownSid(
            WinBuiltinAdministratorsSid,
            NULL,
            &Owner.sid,
            &OwnerSidSize));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlCreateWellKnownSid(
            WinBuiltinPowerUsersSid,
            NULL,
            &Group.sid,
            &GroupSidSize));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwDaclSize = ACL_HEADER_SIZE +
        dwAllowUserCount * (sizeof(ACCESS_ALLOWED_ACE) + SID_MAX_SIZE) +
        dwDenyUserCount * (sizeof(ACCESS_DENIED_ACE) + SID_MAX_SIZE) +
        RtlLengthSid(&Owner.sid) + RtlLengthSid(&Group.sid);

    dwError = LwAllocateMemory(
        dwDaclSize,
        OUT_PPVOID(&pDacl));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlCreateAcl(pDacl, dwDaclSize, ACL_REVISION));
    BAIL_ON_LWUTIL_ERROR(dwError);

    for (dwIndex = 0; dwIndex < dwDenyUserCount; dwIndex++)
    {
        dwError = MapNameToSid(hLsa, ppwszDenyUsers[dwIndex], &pSid);
        if (dwError != LW_ERROR_SUCCESS)
        {
            dwError = MapBuiltinNameToSid(&pSid, ppwszDenyUsers[dwIndex]);
        }

        BAIL_ON_LWUTIL_ERROR(dwError);

        dwError = LwNtStatusToWin32Error(
            RtlAddAccessDeniedAceEx(
                pDacl,
                ACL_REVISION,
                0,
                FILE_ALL_ACCESS,
                pSid));
        BAIL_ON_LWUTIL_ERROR(dwError);

        RTL_FREE(&pSid);
    }

   for (dwIndex = 0; dwIndex < dwAllowUserCount; dwIndex++)
    {
        dwError = MapNameToSid(hLsa, ppwszAllowUsers[dwIndex], &pSid);
        if (dwError != LW_ERROR_SUCCESS)
        {
            dwError = MapBuiltinNameToSid(&pSid, ppwszAllowUsers[dwIndex]);
        }
        BAIL_ON_LWUTIL_ERROR(dwError);

        dwError = LwNtStatusToWin32Error(
            RtlAddAccessAllowedAceEx(
                pDacl,
                ACL_REVISION,
                0,
                mask,
                pSid));
        BAIL_ON_LWUTIL_ERROR(dwError);

        RTL_FREE(&pSid);
    }

    dwError = LwAllocateMemory(
        SECURITY_DESCRIPTOR_ABSOLUTE_MIN_SIZE,
        OUT_PPVOID(&pAbsolute));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlCreateSecurityDescriptorAbsolute(
            pAbsolute,
            SECURITY_DESCRIPTOR_REVISION));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlSetOwnerSecurityDescriptor(
            pAbsolute,
            &Owner.sid,
            FALSE));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlSetGroupSecurityDescriptor(
            pAbsolute,
            &Group.sid,
            FALSE));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlSetDaclSecurityDescriptor(
            pAbsolute,
            TRUE,
            pDacl,
            FALSE));
    BAIL_ON_LWUTIL_ERROR(dwError);

    RtlAbsoluteToSelfRelativeSD(
        pAbsolute,
        NULL,
        &ulRelativeSize);

    dwError = LwAllocateMemory(ulRelativeSize, OUT_PPVOID(&pRelative));
    BAIL_ON_LWUTIL_ERROR(dwError);

    dwError = LwNtStatusToWin32Error(
        RtlAbsoluteToSelfRelativeSD(
            pAbsolute,
            pRelative,
            &ulRelativeSize));
    BAIL_ON_LWUTIL_ERROR(dwError);

    *ppRelative = pRelative;
    *pdwRelativeSize = ulRelativeSize;

cleanup:

    if (hLsa)
    {
        LsaCloseServer(hLsa);
    }

    RTL_FREE(&pSid);
    LW_SAFE_FREE_MEMORY(pDacl);
    LW_SAFE_FREE_MEMORY(pAbsolute);

    return dwError;

error:

    *ppRelative = NULL;
    *pdwRelativeSize = 0;

    LW_SAFE_FREE_MEMORY(pRelative);

    goto cleanup;
}
