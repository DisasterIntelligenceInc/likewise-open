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

#include "domainjoin.h"
#include "djauditing.h"
#include <djmodule.h>


CENTERROR
DJConfigureEventFwd(
    const char * testPrefix,
    BOOLEAN enable
    )
{
    CENTERROR ceError = CENTERROR_SUCCESS;
    LWException *innerExc = NULL;

    if(enable)
        DJ_LOG_INFO("Configuring Likewise Enterprise to run eventfwdd daemon");
    else
        DJ_LOG_INFO("Deconfiguring Likewise Enterprise from running eventfwdd daemon");

    if (geteuid() == 0)
    {
        DJManageDaemon("eventfwdd", enable, 21, 9, &innerExc);
        if (!LW_IS_OK(innerExc) && innerExc->code != CENTERROR_DOMAINJOIN_MISSING_DAEMON)
        {
            DJLogException(LOG_LEVEL_WARNING, innerExc);
        }
    }
    else
    {
        ceError = CENTERROR_DOMAINJOIN_NON_ROOT_USER;
        GOTO_CLEANUP_ON_CENTERROR(ceError);
    }

cleanup:
    return ceError;
}

CENTERROR
DJConfigureReapSyslog(
    const char * testPrefix,
    BOOLEAN enable
    )
{
    CENTERROR ceError = CENTERROR_SUCCESS;
    LWException *innerExc = NULL;

    if(enable)
        DJ_LOG_INFO("Configuring Likewise Enterprise to run reapsysld daemon");
    else
        DJ_LOG_INFO("Deconfiguring Likewise Enterprise from running reapsysld daemon");

    if (geteuid() == 0)
    {
        DJManageDaemon("reapsysld", enable, 12, 9, &innerExc);
        if (!LW_IS_OK(innerExc) && innerExc->code != CENTERROR_DOMAINJOIN_MISSING_DAEMON)
        {
            DJLogException(LOG_LEVEL_WARNING, innerExc);
        }
    }
    else
    {
        ceError = CENTERROR_DOMAINJOIN_NON_ROOT_USER;
        GOTO_CLEANUP_ON_CENTERROR(ceError);
    }

cleanup:
    return ceError;
}

