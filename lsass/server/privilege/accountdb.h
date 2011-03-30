/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*-
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * Editor Settings: expandtabs and use 4 spaces for indentation */

/*
 * Copyright Likewise Software    2004-2011
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
 *        accountdb.h
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Local Authentication Provider
 *
 *        LSA Accounts database
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#ifndef __LSASRV_PRIVS_ACCOUNTDB_H__
#define __LSASRV_PRIVS_ACCOUNTDB_H__


DWORD
LsaSrvCreateAccountsDb(
    IN HANDLE hRegistry,
    IN HKEY hAccountsKey,
    OUT PLW_HASH_TABLE *ppAccounts
    );


DWORD
LsaSrvGetAccountEntry(
    IN PSID pAccountSid,
    OUT PLSA_ACCOUNT *ppAccountEntry
    );


DWORD
LsaSrvSetAccountEntry(
    IN PSID pAccountSid,
    IN PLSA_ACCOUNT pAccountEntry,
    IN BOOLEAN Overwrite
    );


DWORD
LsaSrvAddAccount_inlock(
    IN PSID pAccountSid,
    IN PLSA_ACCOUNT pAccountEntry
    );


DWORD
LsaSrvUpdateAccount_inlock(
    IN PSID pAccountSid,
    IN PLSA_ACCOUNT pAccountEntry
    );


DWORD
LsaSrvPrivsGetAccountEntries(
    IN OUT PDWORD pResume,
    IN DWORD PreferredMaxSize,
    OUT PLSA_ACCOUNT **pppAccounts,
    OUT PDWORD pCount
    );


VOID
LsaSrvPrivsReleaseAccountEntry(
    PLSA_ACCOUNT pEntry
    );


VOID
LsaSrvPrivsAcquireAccountEntry(
    PLSA_ACCOUNT pEntry
    );


VOID
LsaSrvPrivsCheckIfDeleteAccount(
    PLSA_ACCOUNT pAccount
    );


#endif /* __LSASRV_PRIVS_ACCOUNTDB_H__ */
