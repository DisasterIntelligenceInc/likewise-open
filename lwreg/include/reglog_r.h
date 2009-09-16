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
 *        reglog_r.h
 *
 * Abstract:
 *
 *        Registry
 *
 *        Logging API
 *
 * 		  Global variables
 *
 * Authors: Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Marc Guy (mguy@likewisesoftware.com)
 *
 */

#ifndef __REGLOG_R_H__
#define __REGLOG_R_H__

DWORD
RegInitLogging_r(
    PCSTR         pszProgramName,
    RegLogTarget  logTarget,
    RegLogLevel   maxAllowedLogLevel,
    PCSTR         pszPath
    );

DWORD
RegLogGetInfo_r(
    PREG_LOG_INFO* ppLogInfo
    );

DWORD
RegLogSetInfo_r(
    PREG_LOG_INFO pLogInfo
    );

DWORD
RegShutdownLogging_r(
    VOID
    );

DWORD
RegInitTracing_r(
    VOID
    );

DWORD
RegTraceSetFlag_r(
    DWORD   dwTraceFlag,
    BOOLEAN bStatus
    );

DWORD
RegTraceGetInfo_r(
    DWORD    dwTraceFlag,
    PBOOLEAN pbStatus
    );

VOID
RegShutdownTracing_r(
    VOID
    );

#endif /* __REGLOG_R_H__ */
