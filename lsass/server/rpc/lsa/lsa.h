/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

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
 * Abstract: Lsa rpc server functions (rpc server library)
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */

#ifndef _LSA_H_
#define _LSA_H_


NTSTATUS
LsaClose(
    handle_t b,
    PolicyHandle *h
    );


NTSTATUS
LsaOpenPolicy2(
    handle_t b,
    wchar16_t *system_name,
    ObjectAttribute *attrib,
    uint32 access_mask,
    PolicyHandle *phLsa
    );


NTSTATUS
LsaLookupNames(
    handle_t b,
    PolicyHandle *h,
    uint32 num_names,
    UnicodeString *names,
    RefDomainList **domains,
    TranslatedSidArray *sids,
    uint16 level,
    uint32 *count
    );


NTSTATUS
LsaLookupSids(
    handle_t b,
    PolicyHandle *h,
    SidArray *sids,
    RefDomainList **domains,
    TranslatedNameArray *names,
    uint16 level,
    uint32 *count
    );


NTSTATUS LsaLookupNames2(
    handle_t b,
    PolicyHandle *h,
    uint32 num_names,
    UnicodeStringEx *names,
    RefDomainList **domains,
    TranslatedSidArray2 *sids,
    uint16 level,
    uint32 *count,
    uint32 unknown1,
    uint32 unknown2
    );


NTSTATUS
LsaQueryInfoPolicy(
    handle_t b,
    PolicyHandle *h,
    uint16 level,
    LsaPolicyInformation **info
    );


NTSTATUS
LsaQueryInfoPolicy2(
    handle_t b,
    PolicyHandle *h,
    uint16 level,
    LsaPolicyInformation **info
    );


#endif /* _LSA_H_ */
