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
 *        krbtgt.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 * 
 *        Kerberos 5 runtime environment
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Rafal Szczesniak <rszczesniak@likewisesoftware.com>
 *
 */

#include "includes.h"
#include "krbtgt_p.h"

DWORD
LwKrb5GetTgt(
    PCSTR  pszUserPrincipal,
    PCSTR  pszPassword,
    PCSTR  pszCcPath,
    PDWORD pdwGoodUntilTime
    )
{
    DWORD dwError = LW_ERROR_SUCCESS;
    krb5_error_code ret = 0;
    krb5_context ctx = NULL;
    krb5_ccache cc = NULL;
    krb5_get_init_creds_opt opts;
    krb5_principal client = NULL;
    krb5_principal existing_client = NULL;
    krb5_creds creds = {0};
    PSTR pszPass = NULL;
    PSTR pszUPN = NULL;
    PSTR pszRealmIdx = NULL;
    BOOLEAN bUnlockExistingClientLock = FALSE;

    dwError = LwAllocateString(
                    pszUserPrincipal,
                    &pszUPN);
    BAIL_ON_LW_ERROR(dwError);

    if ((pszRealmIdx = strchr(pszUPN, '@')) == NULL) {
        dwError = LW_ERROR_INVALID_PARAMETER;
        BAIL_ON_LW_ERROR(dwError);
    }

    LwStrToUpper(++pszRealmIdx);

    if (LwKrb5RealmIsOffline(pszRealmIdx))
    {
        dwError = LW_ERROR_DOMAIN_IS_OFFLINE;
        BAIL_ON_LW_ERROR(dwError);
    }

    ret = krb5_init_context(&ctx);
    BAIL_ON_KRB_ERROR(ctx, ret);

    krb5_get_init_creds_opt_init(&opts);
    krb5_get_init_creds_opt_set_tkt_life(&opts, LW_KRB5_DEFAULT_TKT_LIFE);
    krb5_get_init_creds_opt_set_forwardable(&opts, TRUE);

    if (LW_IS_NULL_OR_EMPTY_STR(pszCcPath)) {
        ret = krb5_cc_default(ctx, &cc);
        BAIL_ON_KRB_ERROR(ctx, ret);
                
    } else {
        ret = krb5_cc_resolve(ctx, pszCcPath, &cc);
        BAIL_ON_KRB_ERROR(ctx, ret);
    }

    ret = krb5_parse_name(ctx, pszUPN, &client);
    BAIL_ON_KRB_ERROR(ctx, ret);
 
    dwError = pthread_mutex_lock(&gLwKrb5State.ExistingClientLock);
    BAIL_ON_LW_ERROR(dwError);
    bUnlockExistingClientLock = TRUE;
    
    ret = krb5_cc_get_principal(ctx, cc, &existing_client);
    if (ret ||
        (FALSE == krb5_principal_compare(ctx, existing_client, client)))
    {
        ret = krb5_cc_initialize(ctx, cc, client);
        BAIL_ON_KRB_ERROR(ctx, ret);
    }
    
    if (!LW_IS_NULL_OR_EMPTY_STR(pszPassword)) {
        dwError = LwAllocateString(pszPassword, &pszPass);
        BAIL_ON_LW_ERROR(dwError);
    }

    ret = krb5_get_init_creds_password(ctx, &creds, client, pszPass, NULL,
                                       NULL, 0, NULL, &opts);
    BAIL_ON_KRB_ERROR(ctx, ret);

    if (ret == KRB5KRB_AP_ERR_SKEW) {

        ret = krb5_get_init_creds_password(ctx, &creds, client, pszPass, NULL,
                                           NULL, 0, NULL, &opts);
        BAIL_ON_KRB_ERROR(ctx, ret);

    } else {
        BAIL_ON_KRB_ERROR(ctx, ret);
    }

    /* Blow away the old credentials cache so that the old TGT is removed.
     * Otherwise, the new TGT will be stored at the end of the credentials
     * cache, and kerberos will use the old TGT instead.
     *
     * See bug 6908.
     */
    ret = krb5_cc_initialize(ctx, cc, client);
    BAIL_ON_KRB_ERROR(ctx, ret);

    ret = krb5_cc_store_cred(ctx, cc, &creds);
    BAIL_ON_KRB_ERROR(ctx, ret);
    
    if (pdwGoodUntilTime)
    {
    	*pdwGoodUntilTime = creds.times.endtime;
    }

cleanup:

    if (bUnlockExistingClientLock)
    {
        pthread_mutex_unlock(&gLwKrb5State.ExistingClientLock);
    }

    if (ctx) {
        
        if (client)
        {
            krb5_free_principal(ctx, client);
        }
        
        if (existing_client)
        {
            krb5_free_principal(ctx, existing_client);
        }

        krb5_cc_close(ctx, cc);

        krb5_free_cred_contents(ctx, &creds);
        
        krb5_free_context(ctx);
    }

    LW_SAFE_FREE_STRING(pszUPN);
    LW_SAFE_CLEAR_FREE_STRING(pszPass);

    return dwError;
    
error:

    if (pdwGoodUntilTime)
    {
    	*pdwGoodUntilTime = 0;
    }

    if (KRB5_KDC_UNREACH == ret)
    {
        LwKrb5RealmTransitionOffline(pszRealmIdx);
        dwError = LW_ERROR_DOMAIN_IS_OFFLINE;
    }

    goto cleanup;
}


DWORD
LwKrb5GetTgs(
    PCSTR pszCliPrincipal,
    PCSTR pszSvcPrincipal,
    PSTR pszCcPath
    )
{
    DWORD dwError = LW_ERROR_SUCCESS;
    krb5_error_code ret = 0;
    krb5_context ctx = NULL;
    krb5_ccache cc = NULL;
    krb5_flags opts = 0;
    krb5_principal client = NULL;
    krb5_principal service = NULL;
    krb5_creds tgs_req = {0};
    krb5_creds* tgs_rep = NULL;

    ret = krb5_init_context(&ctx);
    BAIL_ON_KRB_ERROR(ctx, ret);

    if (LW_IS_NULL_OR_EMPTY_STR(pszCcPath)) {
        ret = krb5_cc_default(ctx, &cc);
        BAIL_ON_KRB_ERROR(ctx, ret);
                
    } else {
        ret = krb5_cc_resolve(ctx, pszCcPath, &cc);
        BAIL_ON_KRB_ERROR(ctx, ret);
    }

    ret = krb5_parse_name(ctx, pszCliPrincipal, &client);
    BAIL_ON_KRB_ERROR(ctx, ret);

    ret = krb5_parse_name(ctx, pszSvcPrincipal, &service);
    BAIL_ON_KRB_ERROR(ctx, ret);

    tgs_req.client        = client;
    tgs_req.server        = service;
    tgs_req.times.endtime = time(NULL) + LW_KRB5_DEFAULT_TKT_LIFE;

    ret = krb5_get_credentials(ctx, opts, cc, &tgs_req, &tgs_rep);
    BAIL_ON_KRB_ERROR(ctx, ret);

cleanup:

    if (ctx)
    {

        if (client) {
            krb5_free_principal(ctx, client);
        }

        if (service) {
            krb5_free_principal(ctx, service);
        }

        if (tgs_rep) {
            krb5_free_creds(ctx, tgs_rep);
        }

        if (cc) {
            krb5_cc_close(ctx, cc);
        }

        krb5_free_context(ctx);
    }

    return dwError;

error:

    goto cleanup;
}


DWORD
LwKrb5GetServiceTicketForUser(
    uid_t         uid,
    PCSTR         pszUserPrincipal,
    PCSTR         pszServername,
    PCSTR         pszDomain,
    Krb5CacheType cacheType
    )
{
    DWORD dwError = 0;
    krb5_error_code ret = 0;
    krb5_context    ctx = NULL;
    krb5_ccache     cc  = NULL;
    krb5_creds      in_creds = {0};
    krb5_creds*     pCreds = NULL;
    krb5_principal  user_principal = NULL;
    krb5_principal  server_principal = NULL;
    PSTR            pszCachePath = NULL;
    PSTR            pszTargetName = NULL;
    PSTR            pszUPN = NULL;
    PSTR            pszRealmIdx = NULL;
    
    LW_BAIL_ON_INVALID_STRING(pszUserPrincipal);
    LW_BAIL_ON_INVALID_STRING(pszServername);

    dwError = LwAllocateString(
                    pszUserPrincipal,
                    &pszUPN);
    BAIL_ON_LW_ERROR(dwError);

    if ((pszRealmIdx = index(pszUPN, '@')) == NULL) {
        dwError = LW_ERROR_INVALID_PARAMETER;
        BAIL_ON_LW_ERROR(dwError);
    }

    LwStrToUpper(++pszRealmIdx);

    ret = krb5_init_context(&ctx);
    BAIL_ON_KRB_ERROR(ctx, ret);
    
    dwError = LwKrb5GetUserCachePath(
                    uid,
                    cacheType,
                    &pszCachePath);
    BAIL_ON_LW_ERROR(dwError);
            
    if (LW_IS_NULL_OR_EMPTY_STR(pszCachePath)) {
                
        ret = krb5_cc_default(ctx, &cc);
        BAIL_ON_KRB_ERROR(ctx, ret);
                
    } else {
                
        ret = krb5_cc_resolve(ctx, pszCachePath, &cc);
        BAIL_ON_KRB_ERROR(ctx, ret);
                
    }
    
    ret = krb5_parse_name(ctx, pszUPN, &user_principal);
    BAIL_ON_KRB_ERROR(ctx, ret);
    
    dwError = LwAllocateStringPrintf(
                    &pszTargetName,
                    "%s$@%s",
                    pszServername,
                    pszDomain);
    BAIL_ON_LW_ERROR(dwError);
    
    LwStrToUpper(pszTargetName);

    ret = krb5_parse_name(ctx, pszTargetName, &server_principal);
    BAIL_ON_KRB_ERROR(ctx, ret);
    
    ret = krb5_copy_principal(ctx, user_principal, &in_creds.client);
    BAIL_ON_KRB_ERROR(ctx, ret);

    ret = krb5_copy_principal(ctx, server_principal, &in_creds.server);
    BAIL_ON_KRB_ERROR(ctx, ret);
    
    ret = krb5_get_credentials(
                ctx,
                0,
                cc,
                &in_creds,
                &pCreds);
    BAIL_ON_KRB_ERROR(ctx, ret);
    
cleanup:

    if (ctx) {
    
        if (user_principal) {
            krb5_free_principal(ctx, user_principal);
        }
        
        if (server_principal) {
            krb5_free_principal(ctx, server_principal);
        }
    
        if (cc) {
            krb5_cc_close(ctx, cc);
        }
    
        krb5_free_cred_contents(ctx, &in_creds);
        
        if (pCreds) {
            krb5_free_creds(ctx, pCreds);
        }
    
        krb5_free_context(ctx);
    }

    LW_SAFE_FREE_STRING(pszCachePath);
    LW_SAFE_FREE_STRING(pszTargetName);
    LW_SAFE_FREE_STRING(pszUPN);
    
    return dwError;
    
error:

    goto cleanup;
}

static
BOOLEAN
LwKrb5RealmIsOffline(
    IN PCSTR pszRealm
    )
{
    BOOLEAN bIsOffline = FALSE;

    if (pszRealm && gLwKrb5State.pfIsOfflineCallback)
    {
        bIsOffline = gLwKrb5State.pfIsOfflineCallback(pszRealm);
    }

    return bIsOffline;
}

VOID
LwKrb5RealmTransitionOffline(
    IN PCSTR pszRealm
    )
{
    if (pszRealm && gLwKrb5State.pfTransitionOfflineCallback)
    {
        gLwKrb5State.pfTransitionOfflineCallback(pszRealm);
    }
}

/*
local variables:
mode: c
c-basic-offset: 4
indent-tabs-mode: nil
tab-width: 4
end:
*/
