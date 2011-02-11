/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

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
 *        fcb.h
 *
 * Abstract:
 *
 *        Likewise Posix File System Driver (PVFS)
 *
 *        File Control Block routines
 *
 * Authors: Gerald Carter <gcarter@likewise.com>
 */

#ifndef _PVFS_FCB_H
#define _PVFS_FCB_H

#include "pvfs.h"

NTSTATUS
PvfsAllocateFCB(
    PPVFS_FCB *ppFcb
    );

PPVFS_FCB
PvfsReferenceFCB(
    IN PPVFS_FCB pFcb
    );

VOID
PvfsReleaseFCB(
    PPVFS_FCB *ppFcb
    );

NTSTATUS
PvfsCreateFCB(
    OUT PPVFS_FCB *ppFcb,
    IN PCSTR pszFilename,
    IN BOOLEAN bCheckShareAccess,
    IN FILE_SHARE_FLAGS SharedAccess,
    IN ACCESS_MASK DesiredAccess
    );

NTSTATUS
PvfsAddSCBToFCB(
    PPVFS_FCB pFcb,
    PPVFS_SCB pScb
    );

NTSTATUS
PvfsRemoveSCBFromFCB(
    PPVFS_FCB pFcb,
    PPVFS_SCB pScb
    );

NTSTATUS
PvfsRenameFCB(
    PPVFS_FCB pFcb,
    PPVFS_CCB pCcb,
    PPVFS_FILE_NAME pNewFilename
    );

BOOLEAN
PvfsFcbIsPendingDelete(
    PPVFS_FCB pFcb
    );

VOID
PvfsFcbSetPendingDelete(
    PPVFS_FCB pFcb,
    BOOLEAN bPendingDelete
    );

PPVFS_FCB
PvfsGetParentFCB(
    PPVFS_FCB pFcb
    );

LONG64
PvfsClearLastWriteTimeFCB(
    PPVFS_FCB pFcb
    );

VOID
PvfsSetLastWriteTimeFCB(
    PPVFS_FCB pFcb,
    LONG64 LastWriteTime
    );


#endif   /* _PVFS_FCB_H */

/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
