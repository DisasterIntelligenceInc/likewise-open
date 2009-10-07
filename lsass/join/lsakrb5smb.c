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
 *        common.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Join to Active Directory
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Kyle Stemen (kstemen@likewisesoftware.com)
 */

#include "includes.h"

typedef struct _LSA_CREDS_FREE_INFO
{
    krb5_context ctx;
    krb5_ccache cc;
    LW_PIO_CREDS hCreds;
} LSA_CREDS_FREE_INFO;

DWORD
LsaSetSMBCreds(
    IN PCSTR pszDomain,
    IN PCSTR pszUsername,
    IN PCSTR pszPassword,
    IN BOOLEAN bSetDefaultCachePath,
    OUT PLSA_CREDS_FREE_INFO* ppFreeInfo,
    OUT OPTIONAL LW_PIO_CREDS* ppOldToken
    )
{
    DWORD dwError = 0;
    krb5_error_code ret = 0;
    PSTR pszNewCachePath = NULL;
    PCSTR  pszCacheName = NULL;
    PCSTR  pszCacheType = NULL;
    krb5_context ctx = 0;
    krb5_ccache cc = 0;
    LW_PIO_CREDS pNewCreds = NULL;
    LW_PIO_CREDS pOldCreds = NULL;
    PLSA_CREDS_FREE_INFO pFreeInfo = NULL;

    BAIL_ON_INVALID_POINTER(ppFreeInfo);
    BAIL_ON_INVALID_STRING(pszDomain);
    BAIL_ON_INVALID_STRING(pszUsername);

    ret = krb5_init_context(&ctx);
    BAIL_ON_KRB_ERROR(ctx, ret);

    /* Generates a new filed based credentials cache in /tmp. The file will
     * be owned by root and only accessible by root.
     */
    ret = krb5_cc_new_unique(
            ctx,
            "FILE",
            "hint",
            &cc);
    BAIL_ON_KRB_ERROR(ctx, ret);

    pszCacheType = krb5_cc_get_type(ctx, cc);
    pszCacheName = krb5_cc_get_name(ctx, cc);
    dwError = LwAllocateStringPrintf(&pszNewCachePath, "%s:%s", pszCacheType, pszCacheName);
    BAIL_ON_LSA_ERROR(dwError);

    if (bSetDefaultCachePath)
    {
        dwError = LwKrb5SetDefaultCachePath(
                  pszNewCachePath,
                  NULL);
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LwKrb5GetTgt(
                pszUsername,
                pszPassword,
                pszNewCachePath,
                NULL);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwIoCreateKrb5CredsA(
        pszUsername,
        pszNewCachePath,
        &pNewCreds);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwAllocateMemory(sizeof(*pFreeInfo), (PVOID*)&pFreeInfo);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwIoGetThreadCreds(&pOldCreds);
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LwIoSetThreadCreds(pNewCreds);
    BAIL_ON_LSA_ERROR(dwError);

    pFreeInfo->ctx = ctx;
    pFreeInfo->cc = cc;
    pFreeInfo->hCreds = pNewCreds;
    pNewCreds = NULL;

cleanup:
    *ppFreeInfo = pFreeInfo;
    if (ppOldToken)
    {
        *ppOldToken = pOldCreds;
    }
    else
    {
        if (pOldCreds != NULL)
        {
            LwIoDeleteCreds(pOldCreds);
        }
    }

    if (pNewCreds != NULL)
    {
        LwIoDeleteCreds(pNewCreds);
    }
    LW_SAFE_FREE_STRING(pszNewCachePath);

    return dwError;

error:
    if (ctx != NULL)
    {
        if (cc != NULL)
        {
            krb5_cc_destroy(ctx, cc);
        }
        krb5_free_context(ctx);
    }

    if (pFreeInfo)
    {
        LwFreeMemory(pFreeInfo);
        pFreeInfo = NULL;
    }

    if (pOldCreds != NULL)
    {
        LwIoDeleteCreds(pOldCreds);
        pOldCreds = NULL;
    }

    goto cleanup;
}

void
LsaFreeSMBCreds(
    IN OUT PLSA_CREDS_FREE_INFO* ppFreeInfo
    )
{
    PLSA_CREDS_FREE_INFO pFreeInfo = *ppFreeInfo;

    if (!pFreeInfo)
    {
        goto cleanup;
    }

    if (pFreeInfo->hCreds != NULL)
    {
        LwIoSetThreadCreds(pFreeInfo->hCreds);
        LwIoDeleteCreds(pFreeInfo->hCreds);
    }

    if (pFreeInfo->ctx != NULL)
    {
        if (pFreeInfo->cc != NULL)
        {
            krb5_cc_destroy(pFreeInfo->ctx, pFreeInfo->cc);
        }
        krb5_free_context(pFreeInfo->ctx);
    }

    LwFreeMemory(pFreeInfo);

    *ppFreeInfo = NULL;

cleanup:
    return;
}
