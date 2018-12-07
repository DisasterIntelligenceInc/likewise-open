/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

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
 *        vmcachespi.h
 *
 * Abstract:
 *
 *        This is the public interface for the Vmdir Provider Local Cache
 *
 * Authors: Kyle Stemen (kstemen@likewisesoftware.com)
 *          Krishna Ganugapati (krishnag@likewisesoftware.com)
 *
 */

#ifndef __VMCACHESPI_H__
#define __VMCACHESPI_H__

typedef
DWORD
(*PFNOpenHandle)(
    IN PCSTR pszDbPath,
    IN PLSA_VMDIR_PROVIDER_STATE pState,
    OUT PLSA_DB_HANDLE phDb
    );

typedef
void
(*PFNSafeClose)(
    PLSA_DB_HANDLE phDb
    );

typedef
DWORD
(*PFNFlushToDisk)(
    LSA_DB_HANDLE hDb
    );

typedef
DWORD
(*PFNFindUserByName)(
    LSA_DB_HANDLE hDb,
    PLSA_LOGIN_NAME_INFO pUserNameInfo,
    PLSA_SECURITY_OBJECT* ppObject
    );

typedef
DWORD
(*PFNFindUserById)(
    LSA_DB_HANDLE hDb,
    uid_t uid,
    PLSA_SECURITY_OBJECT* ppObject
    );

typedef
DWORD
(*PFNFindGroupByName)(
    LSA_DB_HANDLE hDb,
    PLSA_LOGIN_NAME_INFO pGroupNameInfo,
    PLSA_SECURITY_OBJECT* ppObject
    );

typedef
DWORD
(*PFNFindGroupById)(
    LSA_DB_HANDLE hDb,
    gid_t gid,
    PLSA_SECURITY_OBJECT* ppObject
    );

typedef
DWORD
(*PFNRemoveUserBySid)(
    IN LSA_DB_HANDLE hDb,
    IN PCSTR pszSid
    );

typedef
DWORD
(*PFNRemoveGroupBySid)(
    IN LSA_DB_HANDLE hDb,
    IN PCSTR pszSid
    );

typedef
DWORD
(*PFNEmptyCache)(
    IN LSA_DB_HANDLE hDb
    );

typedef
DWORD
(*PFNStoreObjectEntries)(
    LSA_DB_HANDLE hDb,
    size_t  sObjectCount,
    PLSA_SECURITY_OBJECT* ppObjects
    );

typedef
DWORD
(*PFNStoreGroupMembership)(
    IN LSA_DB_HANDLE hDb,
    IN PCSTR pszParentSid,
    IN size_t sMemberCount,
    IN PLSA_GROUP_MEMBERSHIP* ppMembers
    );

typedef
DWORD
(*PFNStoreGroupsForUser)(
    IN LSA_DB_HANDLE hDb,
    IN PCSTR pszChildSid,
    IN size_t sMemberCount,
    IN PLSA_GROUP_MEMBERSHIP* ppMembers,
    IN BOOLEAN bIsPacAuthoritative
    );

typedef
DWORD
(*PFNGetMemberships)(
    IN LSA_DB_HANDLE hDb,
    IN PCSTR pszSid,
    IN BOOLEAN bIsGroupMembers,
    IN BOOLEAN bFilterNotInPacNorLdap,
    OUT size_t* psCount,
    OUT PLSA_GROUP_MEMBERSHIP** pppResults
    );

typedef
DWORD
(*PFNEnumUsersCache)(
    IN LSA_DB_HANDLE           hDb,
    IN DWORD                   dwMaxNumUsers,
    IN PCSTR                   pszResume,
    OUT DWORD*                 dwNumUsersFound,
    OUT PLSA_SECURITY_OBJECT** pppObjects
    );

typedef
DWORD
(*PFNEnumGroupsCache)(
    IN LSA_DB_HANDLE           hDb,
    IN DWORD                   dwMaxNumGroups,
    IN PCSTR                   pszResume,
    OUT DWORD*                 dwNumGroupsFound,
    OUT PLSA_SECURITY_OBJECT** pppObjects
    );

typedef
DWORD
(*PFNFindObjectByDN)(
    LSA_DB_HANDLE hDb,
    PCSTR pszDN,
    PLSA_SECURITY_OBJECT *ppObject
    );

typedef
DWORD
(*PFNFindObjectsByDNList)(
    IN LSA_DB_HANDLE hDb,
    IN size_t sCount,
    IN PSTR* ppszDnList,
    OUT PLSA_SECURITY_OBJECT** pppResults
    );

typedef
DWORD
(*PFNFindObjectBySid)(
    LSA_DB_HANDLE hDb,
    PCSTR pszSid,
    PLSA_SECURITY_OBJECT *ppObject
    );

typedef
DWORD
(*PFNFindObjectsBySidList)(
    IN LSA_DB_HANDLE hDb,
    IN size_t sCount,
    IN PSTR* ppszSidList,
    OUT PLSA_SECURITY_OBJECT** pppResults
    );

typedef struct __VMCACHE_PROVIDER_FUNCTION_TABLE
{
    PFNOpenHandle               pfnOpenHandle;
    PFNSafeClose                pfnSafeClose;
    PFNFindUserByName           pfnFindUserByName;
    PFNFindUserById             pfnFindUserById;
    PFNFindGroupByName          pfnFindGroupByName;
    PFNFindGroupById            pfnFindGroupById;
    PFNEmptyCache               pfnEmptyCache;
    PFNStoreObjectEntries       pfnStoreObjectEntries;
    PFNStoreGroupMembership     pfnStoreGroupMembership;
    PFNStoreGroupsForUser       pfnStoreGroupsForUser;
    PFNGetMemberships           pfnGetMemberships;
    PFNEnumUsersCache           pfnEnumUsersCache;
    PFNEnumGroupsCache          pfnEnumGroupsCache;
    PFNFindObjectByDN           pfnFindObjectByDN;
    PFNFindObjectsByDNList      pfnFindObjectsByDNList;
    PFNFindObjectBySid          pfnFindObjectBySid;
    PFNFindObjectsBySidList     pfnFindObjectsBySidList;
} VMCACHE_PROVIDER_FUNCTION_TABLE, *PVMCACHE_PROVIDER_FUNCTION_TABLE;

#define VMCACHE_SYMBOL_NAME_INITIALIZE_PROVIDER "VmCacheInitializeProvider"

typedef DWORD (*PFNInitializeCacheProvider)(
                    PSTR* ppszProviderName,
                    PVMCACHE_PROVIDER_FUNCTION_TABLE* ppFnTable
                    );

#define VMCACHE_SYMBOL_NAME_SHUTDOWN_PROVIDER "VmCacheShutdownProvider"

typedef DWORD (*PFNShutdownCacheProvider)(
                    PSTR pszProviderName,
                    PVMCACHE_PROVIDER_FUNCTION_TABLE pFnTable
                    );

#endif /* __VMCACHESPI_H__ */
