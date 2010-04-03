/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright Likewise Software
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
 * Abstract: Lsa interface (rpc server library)
 *
 * Authors: Rafal Szczesniak (rafal@likewisesoftware.com)
 */

#include <config.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <pthread.h>

#include <dce/rpc.h>
#include <dce/smb.h>
#include <dce/lrpc.h>
#include <dce/dcethread.h>
#include <wc16str.h>
#include <lw/base.h>
#include <lwsid.h>
#include <lwrpc/allocate.h>
#include <lwrpc/unicodestring.h>
#include <lwrpc/samr.h>
#include <lwrpc/lsa.h>
#include <lwrpc/netlogon.h>
#include <lwio/lwio.h>
#include <lwnet.h>
#include <lwps/lwps.h>
#include <lwkrb5.h>

#include <lsa/lsa.h>
#include <lsaunistr.h>
#include <lsarpcsrv.h>
#include <lsasrvutils.h>
#include <rpcctl-register.h>
#include <directory.h>
#include <samr_srv.h>

#include "lsa_cfg.h"
#include "lsa_srv.h"
#include "lsadefs.h"
#include "structs.h"
#include "lsa_contexthandle.h"
#include "lsa_accesstoken.h"
#include "lsa_security.h"
#include "lsa_memory.h"
#include "lsa_accounts.h"
#include "lsa_domaincache.h"
#include "lsa.h"
#include "lsa_h.h"

#include "externs.h"


/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
