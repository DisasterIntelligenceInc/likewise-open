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
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        srvsvcbinding.h
 *
 * Abstract:
 *
 *        Likewise Server Service (srvsvc) RPC client and server
 *
 *        DCE/RPC binding functions
 *
 * Authors: Rafal Szczesniak (rafal@likewise.com)
 */


#ifndef _SRVSVC_BINDING_H_
#define _SRVSVC_BINDING_H_

#include <lwrpc/types.h>
#include <lwio/lwio.h>

#define SRVSVC_DEFAULT_PROT_SEQ   "ncacn_np"
#define SRVSVC_DEFAULT_ENDPOINT   "\\pipe\\srvsvc"
#define SRVSVC_LOCAL_ENDPOINT     "/var/lib/likewise/rpc/srvsvc"


RPCSTATUS
InitSrvSvcBindingDefault(
    OUT handle_t  *phBinding,
    IN  PCSTR      pszHostname,
    IN  PIO_CREDS  pCreds
    );


RPCSTATUS
InitSrvSvcBindingFull(
    OUT handle_t *phBinding,
    IN  PCSTR     pszProtSeq,
    IN  PCSTR     pszHostname,
    IN  PCSTR     pszEndpoint,
    IN  PCSTR     pszUuid,
    IN  PCSTR     pszOptions,
    IN  PIO_CREDS pCreds
    );


RPCSTATUS
FreeSrvSvcBinding(
    handle_t *phBinding
    );


#endif /* _SRVSVC_BINDING_H_ */


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
