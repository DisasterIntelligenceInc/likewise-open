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

#include "includes.h"

NET_API_STATUS NetShareSetInfo(
    handle_t b,
    const wchar16_t *servername,
    const wchar16_t *netname,
    uint32 level,
    uint8 *bufptr,
    uint32 *parm_err
    )
{
    NET_API_STATUS status = ERROR_SUCCESS;
    srvsvc_NetShareInfo info;
    PSHARE_INFO_502 buf502 = NULL;
    SHARE_INFO_502_I info502;
    PSHARE_INFO_1501 buf1501 = NULL;
    SHARE_INFO_1501_I info1501;
    uint8 *sdbuf = NULL;

    BAIL_ON_INVALID_PTR(b, status);
    BAIL_ON_INVALID_PTR(netname, status);

    memset(&info, 0, sizeof(info));
    memset(&info502, 0, sizeof(info502));
    memset(&info1501, 0, sizeof(info1501));

    switch (level) {
    case 1:
        info.info1 = (PSHARE_INFO_1)bufptr;
        break;
    case 2:
        info.info2 = (PSHARE_INFO_2)bufptr;
        break;
    case 502:
        buf502 = (PSHARE_INFO_502)bufptr;

        if (buf502)
        {
            if ((buf502->shi502_security_descriptor && !buf502->shi502_reserved) ||
                (!buf502->shi502_security_descriptor && buf502->shi502_reserved))
            {
                status = ERROR_INVALID_PARAMETER;
                BAIL_ON_WIN_ERROR(status);
            }

            info502.shi502_netname             = buf502->shi502_netname;
            info502.shi502_type                = buf502->shi502_type;
            info502.shi502_remark              = buf502->shi502_remark;
            info502.shi502_permissions         = buf502->shi502_permissions;
            info502.shi502_max_uses            = buf502->shi502_max_uses;
            info502.shi502_current_uses        = buf502->shi502_current_uses;
            info502.shi502_path                = buf502->shi502_path;
            info502.shi502_password            = buf502->shi502_password;
            info502.shi502_reserved            = buf502->shi502_reserved;
            info502.shi502_security_descriptor = buf502->shi502_security_descriptor;

            info.info502 = &info502;
        }
        break;
    case 1004:
        info.info1004 = (PSHARE_INFO_1004)bufptr;
        break;
    case 1005:
        info.info1005 = (PSHARE_INFO_1005)bufptr;
        break;
    case 1006:
        info.info1006 = (PSHARE_INFO_1006)bufptr;
        break;
    case 1501:
        buf1501 = (PSHARE_INFO_1501)bufptr;

        if (buf1501)
        {
            if ((buf1501->shi1501_security_descriptor && !buf1501->shi1501_reserved) ||
                (!buf1501->shi1501_security_descriptor && buf1501->shi1501_reserved))
            {
                status = ERROR_INVALID_PARAMETER;
                BAIL_ON_WIN_ERROR(status);
            }

            info1501.shi1501_reserved            = buf1501->shi1501_reserved;
            info1501.shi1501_security_descriptor = buf1501->shi1501_security_descriptor;

            info.info1501 = &info1501;
        }
        break;
    }

    DCERPC_CALL(status,
                _NetrShareSetInfo(b,
                                  (wchar16_t *)servername,
                                  (wchar16_t *)netname,
                                  level, info, parm_err));
    BAIL_ON_WIN_ERROR(status);

cleanup:
    SAFE_FREE(sdbuf);
    return status;

error:
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
