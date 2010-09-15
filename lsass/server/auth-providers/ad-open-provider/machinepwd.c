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
 *        machinepwd.c
 *
 * Abstract:
 *
 *        Likewise Security and Authentication Subsystem (LSASS)
 *
 *        Machine Password API
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Brian Dunstan (bdunstan@likewisesoftware.com)
 */

#include "adprovider.h"

#define DEFAULT_THREAD_WAITSECS  (30 * LSA_SECONDS_IN_MINUTE)
#define ERROR_THREAD_WAITSECS  (5 * LSA_SECONDS_IN_MINUTE)

//
// Global Module State Type
//

typedef struct _LSA_MACHINEPWD_STATE {
    BOOLEAN bThreadShutdown;
    DWORD dwThreadWaitSecs;
    pthread_t Thread;
    pthread_t* pThread;
    pthread_mutex_t ThreadLock;
    pthread_mutex_t *pThreadLock;
    pthread_cond_t ThreadCondition;
    pthread_cond_t *pThreadCondition;
    HANDLE hPasswordStore;
    DWORD dwTgtExpiry;
    DWORD dwTgtExpiryGraceSeconds;
} LSA_MACHINEPWD_STATE, *PLSA_MACHINEPWD_STATE;

static
BOOLEAN
ADShouldRefreshMachineTGT(
    IN PLSA_MACHINEPWD_STATE pMachinePwdState
    );

static
PVOID
ADSyncMachinePasswordThreadRoutine(
    PVOID pData
    );

static
VOID
ADLogMachinePWUpdateSuccessEvent(
    VOID
    );

static
VOID
ADLogMachinePWUpdateFailureEvent(
    DWORD dwErrCode
    );

static
VOID
ADLogMachineTGTRefreshSuccessEvent(
    VOID
    );

static
VOID
ADLogMachineTGTRefreshFailureEvent(
    DWORD dwErrCode
    );

static
VOID
ADSetMachineTGTExpiryInternal(
    PLSA_MACHINEPWD_STATE pMachinePwdState,
    DWORD dwGoodUntil,
    DWORD dwThreadWaitSecs
    );

//
// Function Implementations
//

DWORD
ADInitMachinePasswordSync(
    IN PLSA_AD_PROVIDER_STATE pState
    )
{
    DWORD dwError = 0;
    PLSA_MACHINEPWD_STATE pMachinePwdState = NULL;

    dwError = LwAllocateMemory(
                  sizeof(*pMachinePwdState),
                  (PVOID*)&pMachinePwdState);
    BAIL_ON_LSA_ERROR(dwError);

    pMachinePwdState->bThreadShutdown = FALSE;
    pMachinePwdState->dwThreadWaitSecs = DEFAULT_THREAD_WAITSECS;
    pMachinePwdState->dwTgtExpiryGraceSeconds = 2 * DEFAULT_THREAD_WAITSECS;

    dwError = LwMapErrnoToLwError(pthread_mutex_init(&pMachinePwdState->ThreadLock, NULL));
    BAIL_ON_LSA_ERROR(dwError);

    pMachinePwdState->pThreadLock = &pMachinePwdState->ThreadLock;

    dwError = LwMapErrnoToLwError(pthread_cond_init(&pMachinePwdState->ThreadCondition, NULL));
    BAIL_ON_LSA_ERROR(dwError);

    pMachinePwdState->pThreadCondition = &pMachinePwdState->ThreadCondition;

    dwError = LwpsOpenPasswordStore(
                    LWPS_PASSWORD_STORE_DEFAULT,
                    &pMachinePwdState->hPasswordStore);
    BAIL_ON_LSA_ERROR(dwError);

    pState->hMachinePwdState = pMachinePwdState;

cleanup:

    return dwError;

error:

    ADShutdownMachinePasswordSync(pState);

    goto cleanup;
}

DWORD
ADStartMachinePasswordSync(
    IN PLSA_AD_PROVIDER_STATE pState
    )
{
    DWORD dwError = 0;
    PLSA_MACHINEPWD_STATE pMachinePwdState = pState->hMachinePwdState;

    dwError = pthread_create(&pMachinePwdState->Thread,
                             NULL,
                             ADSyncMachinePasswordThreadRoutine,
                             pState);
    BAIL_ON_LSA_ERROR(dwError);

    pMachinePwdState->pThread = &pMachinePwdState->Thread;

cleanup:

    return dwError;

error:

    pMachinePwdState->pThread = NULL;

    goto cleanup;
}

static
DWORD
ADChangeMachinePasswordInThreadLock(
    IN PLSA_AD_PROVIDER_STATE pState
    )
{
    DWORD dwError = 0;

    LSA_LOG_VERBOSE("Changing machine password");

    dwError = AD_SetSystemAccess(NULL);
    if (dwError)
    {
        LSA_LOG_ERROR("Error: Failed to acquire credentials (error = %u)", dwError);
        BAIL_ON_LSA_ERROR(dwError);
    }

    dwError = LsaMachineChangePassword();
    if (dwError)
    {
        LSA_LOG_ERROR("Error: Failed to change machine password (error = %u)", dwError);

        if (AD_EventlogEnabled(pState))
        {
            ADLogMachinePWUpdateFailureEvent(dwError);
        }

        BAIL_ON_LSA_ERROR(dwError);
    }

    if (AD_EventlogEnabled(pState))
    {
        ADLogMachinePWUpdateSuccessEvent();
    }

cleanup:

    return dwError;

error:

    goto cleanup;
}

static
PVOID
ADSyncMachinePasswordThreadRoutine(
    PVOID pData
    )
{
    DWORD dwError = 0;
    PLSA_AD_PROVIDER_STATE pState = (PLSA_AD_PROVIDER_STATE)pData;
    PLSA_MACHINEPWD_STATE pMachinePwdState = (PLSA_MACHINEPWD_STATE)pState->hMachinePwdState;
    DWORD dwPasswordSyncLifetime = 0;
    struct timespec timeout = {0, 0};
    PLWPS_PASSWORD_INFO pAcctInfo = NULL;
    PSTR pszHostname = NULL;
    PSTR pszDnsDomainName = NULL;
    DWORD dwGoodUntilTime = 0;

    LSA_LOG_INFO("Machine Password Sync Thread starting");

    pthread_mutex_lock(pMachinePwdState->pThreadLock);

    for (;;)
    {
        DWORD dwReapingAge = 0;
        DWORD dwCurrentPasswordAge = 0;
        BOOLEAN bRefreshTGT = FALSE;

        if (pMachinePwdState->bThreadShutdown)
        {
           break;
        }

        dwError = LsaDnsGetHostInfo(&pszHostname);
        if (dwError)
        {
            LSA_LOG_ERROR("Error: Failed to find hostname (error = %u)",
                          dwError);
            dwError = 0;
            goto lsa_wait_resync;
        }

        if (!pState->pProviderData)
        {
            dwError = 0;
            goto lsa_wait_resync;
        }
        ADSyncTimeToDC(pState, pState->pProviderData->szDomain);

        dwError = LwpsGetPasswordByHostName(
                        pMachinePwdState->hPasswordStore,
                        pszHostname,
                        &pAcctInfo);
        if (dwError)
        {
            LSA_LOG_ERROR("Error: Failed to get machine password information (error = %u)", dwError);
            dwError = 0;
            goto lsa_wait_resync;
        }

        dwCurrentPasswordAge =
                         difftime(
                              time(NULL),
                              pAcctInfo->last_change_time);

        dwPasswordSyncLifetime = AD_GetMachinePasswordSyncPwdLifetime(pState);
        dwReapingAge = dwPasswordSyncLifetime / 2;

        dwError = AD_MachineCredentialsCacheInitialize(pState);
        if (dwError)
        {
            LSA_LOG_DEBUG("Failed to initialize credentials cache (error = %u)", dwError);
            dwError = 0;
            goto lsa_wait_resync;
        }

        if ((dwReapingAge > 0) && (dwCurrentPasswordAge >= dwReapingAge))
        {
            dwError = ADChangeMachinePasswordInThreadLock(pState);
            if (dwError)
            {
                dwError = 0;
            }
            else
            {
                bRefreshTGT = TRUE;
            }
        }

        if (!bRefreshTGT)
        {
            bRefreshTGT = ADShouldRefreshMachineTGT(pMachinePwdState);
        }

        if (bRefreshTGT)
        {
            dwError = LwKrb5RefreshMachineTGT(&dwGoodUntilTime);
            if (dwError)
            {
                if (AD_EventlogEnabled(pState))
                {
                    ADLogMachineTGTRefreshFailureEvent(dwError);
                }

                LSA_LOG_ERROR("Error: Failed to refresh machine TGT (error = %u)", dwError);

                if (dwError == LW_ERROR_DOMAIN_IS_OFFLINE)
                {
                    LW_SAFE_FREE_STRING(pszDnsDomainName);

                    dwError = LwWc16sToMbs(pAcctInfo->pwszDnsDomainName, &pszDnsDomainName);
                    BAIL_ON_LSA_ERROR(dwError);

                    LsaDmTransitionOffline(
                        pState->hDmState,
                        pszDnsDomainName,
                        FALSE);
                }

                ADSetMachineTGTExpiryError(pMachinePwdState);
                dwError = 0;
                goto lsa_wait_resync;
            }

            ADSetMachineTGTExpiry(pMachinePwdState, dwGoodUntilTime);

            LSA_LOG_VERBOSE("Machine TGT was refreshed successfully");

            if (AD_EventlogEnabled(pState))
            {
                ADLogMachineTGTRefreshSuccessEvent();
            }
        }

lsa_wait_resync:

        if (pAcctInfo)
        {
            LwpsFreePasswordInfo(pMachinePwdState->hPasswordStore, pAcctInfo);
            pAcctInfo = NULL;
        }

        LW_SAFE_FREE_STRING(pszHostname);

        timeout.tv_sec = time(NULL) + pMachinePwdState->dwThreadWaitSecs;
        timeout.tv_nsec = 0;

retry_wait:

        dwError = pthread_cond_timedwait(pMachinePwdState->pThreadCondition,
                                         pMachinePwdState->pThreadLock,
                                         &timeout);

        if (pMachinePwdState->bThreadShutdown)
        {
           break;
        }

        if (dwError == ETIMEDOUT)
        {
            dwError = 0;
            if (time(NULL) < timeout.tv_sec)
            {
                // It didn't really timeout. Something else happened
                goto retry_wait;
            }
        }
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:

    if (pAcctInfo)
    {
        LwpsFreePasswordInfo(pMachinePwdState->hPasswordStore, pAcctInfo);
    }

    LW_SAFE_FREE_STRING(pszHostname);
    LW_SAFE_FREE_STRING(pszDnsDomainName);

    pthread_mutex_unlock(pMachinePwdState->pThreadLock);

    LSA_LOG_INFO("Machine Password Sync Thread stopping");

    return NULL;

error:

    LSA_LOG_ERROR("Machine password sync thread exiting due to error [code: %ld]", dwError);

    goto cleanup;
}

VOID
ADSyncTimeToDC(
    PLSA_AD_PROVIDER_STATE pState,
    PCSTR pszDomainFQDN
    )
{
    DWORD dwError = 0;
    LWNET_UNIX_TIME_T dcTime = 0;
    time_t ttDcTime = 0;

    if ( !AD_ShouldSyncSystemTime(pState) )
    {
        goto cleanup;
    }

    BAIL_ON_INVALID_STRING(pszDomainFQDN);

    if (LsaDmIsDomainOffline(pState->hDmState, pszDomainFQDN))
    {
        goto cleanup;
    }

    dwError = LWNetGetDCTime(
                    pszDomainFQDN,
                    &dcTime);
    BAIL_ON_LSA_ERROR(dwError);

    ttDcTime = (time_t) dcTime;

    if (labs(ttDcTime - time(NULL)) > AD_GetClockDriftSeconds(pState)) {
        dwError = LsaSetSystemTime(ttDcTime);
        BAIL_ON_LSA_ERROR(dwError);
    }

cleanup:

    return;

error:

    LSA_LOG_ERROR("Failed to sync system time [error code: %u]", dwError);

    goto cleanup;
}

VOID
ADShutdownMachinePasswordSync(
    IN PLSA_AD_PROVIDER_STATE pState
    )
{
    PLSA_MACHINEPWD_STATE pMachinePwdState = pState->hMachinePwdState;

    if (pMachinePwdState->pThread)
    {
        pthread_mutex_lock(pMachinePwdState->pThreadLock);
        pMachinePwdState->bThreadShutdown = TRUE;
        pthread_cond_signal(pMachinePwdState->pThreadCondition);
        pthread_mutex_unlock(pMachinePwdState->pThreadLock);

        pthread_join(pMachinePwdState->Thread, NULL);
        pMachinePwdState->pThread = NULL;
        pMachinePwdState->bThreadShutdown = FALSE;
    }

    if (pMachinePwdState->pThreadCondition)
    {
        pthread_cond_destroy(pMachinePwdState->pThreadCondition);
    }

    if (pMachinePwdState->pThreadLock)
    {
        pthread_mutex_destroy(pMachinePwdState->pThreadLock);
    }

    if (pMachinePwdState->hPasswordStore)
    {
        LwpsClosePasswordStore(pMachinePwdState->hPasswordStore);
        pMachinePwdState->hPasswordStore = NULL;
    }

    LW_SAFE_FREE_MEMORY(pState->hMachinePwdState);
}

static
BOOLEAN
ADShouldRefreshMachineTGT(
    IN PLSA_MACHINEPWD_STATE pMachinePwdState
    )
{
    BOOLEAN bRefresh = FALSE;
    BOOLEAN bInLock = FALSE;

    ENTER_AD_GLOBAL_DATA_RW_READER_LOCK(bInLock);

    if (!pMachinePwdState->dwTgtExpiry ||
        (difftime(pMachinePwdState->dwTgtExpiry, time(NULL)) <= pMachinePwdState->dwTgtExpiryGraceSeconds))
    {
        bRefresh = TRUE;
    }

    LEAVE_AD_GLOBAL_DATA_RW_READER_LOCK(bInLock);

    return bRefresh;
}

VOID
ADSetMachineTGTExpiry(
    IN LSA_MACHINEPWD_STATE_HANDLE hMachinePwdState,
    IN DWORD dwGoodUntil
    )
{
    ADSetMachineTGTExpiryInternal(
        hMachinePwdState,
        dwGoodUntil,
        DEFAULT_THREAD_WAITSECS);
}

VOID
ADSetMachineTGTExpiryError(
    IN LSA_MACHINEPWD_STATE_HANDLE hMachinePwdState
    )
{
    ADSetMachineTGTExpiryInternal(
        hMachinePwdState,
        0,
        ERROR_THREAD_WAITSECS);
}

static
VOID
ADSetMachineTGTExpiryInternal(
    PLSA_MACHINEPWD_STATE pMachinePwdState,
    DWORD dwGoodUntil,
    DWORD dwThreadWaitSecs
    )
{
    BOOLEAN bInLock = FALSE;
    DWORD lifetime = 0;

    ENTER_AD_GLOBAL_DATA_RW_WRITER_LOCK(bInLock);

    if (dwGoodUntil)
    {
        pMachinePwdState->dwTgtExpiry = dwGoodUntil;

        lifetime = difftime(
                       pMachinePwdState->dwTgtExpiry,
                       time(NULL));

        pMachinePwdState->dwTgtExpiryGraceSeconds =
            LW_MAX(lifetime / 2, 2 * DEFAULT_THREAD_WAITSECS);
    }

    if (dwThreadWaitSecs)
    {
        pMachinePwdState->dwThreadWaitSecs = dwThreadWaitSecs;
    }
    else
    {
        pMachinePwdState->dwThreadWaitSecs = DEFAULT_THREAD_WAITSECS;
    }

    LEAVE_AD_GLOBAL_DATA_RW_WRITER_LOCK(bInLock);
}

static
VOID
ADLogMachinePWUpdateSuccessEvent(
    VOID
    )
{
    DWORD dwError = 0;
    PSTR pszDescription = NULL;

    dwError = LwAllocateStringPrintf(
                 &pszDescription,
                 "Updated Active Directory machine password.\r\n\r\n" \
                 "     Authentication provider:   %s",
                 LSA_SAFE_LOG_STRING(gpszADProviderName));
    BAIL_ON_LSA_ERROR(dwError);

    LsaSrvLogServiceSuccessEvent(
            LSASS_EVENT_SUCCESSFUL_MACHINE_ACCOUNT_PASSWORD_UPDATE,
            PASSWORD_EVENT_CATEGORY,
            pszDescription,
            NULL);

cleanup:

    LW_SAFE_FREE_STRING(pszDescription);

    return;

error:

    goto cleanup;
}

static
VOID
ADLogMachinePWUpdateFailureEvent(
    DWORD dwErrCode
    )
{
    DWORD dwError = 0;
    PSTR pszDescription = NULL;
    PSTR pszData = NULL;

    dwError = LwAllocateStringPrintf(
                 &pszDescription,
                 "The Active Directory machine password failed to update.\r\n\r\n" \
                 "     Authentication provider:   %s",
                 LSA_SAFE_LOG_STRING(gpszADProviderName));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaGetErrorMessageForLoggingEvent(
                         dwErrCode,
                         &pszData);

    LsaSrvLogServiceFailureEvent(
            LSASS_EVENT_FAILED_MACHINE_ACCOUNT_PASSWORD_UPDATE,
            PASSWORD_EVENT_CATEGORY,
            pszDescription,
            pszData);

cleanup:

    LW_SAFE_FREE_STRING(pszDescription);
    LW_SAFE_FREE_STRING(pszData);

    return;

error:

    goto cleanup;
}

static
VOID
ADLogMachineTGTRefreshSuccessEvent(
    VOID
    )
{
    DWORD dwError = 0;
    PSTR pszDescription = NULL;

    dwError = LwAllocateStringPrintf(
                 &pszDescription,
                 "Refreshed Active Directory machine account TGT (Ticket Granting Ticket).\r\n\r\n" \
                 "     Authentication provider:   %s",
                 LSA_SAFE_LOG_STRING(gpszADProviderName));
    BAIL_ON_LSA_ERROR(dwError);

    LsaSrvLogServiceSuccessEvent(
            LSASS_EVENT_SUCCESSFUL_MACHINE_ACCOUNT_TGT_REFRESH,
            KERBEROS_EVENT_CATEGORY,
            pszDescription,
            NULL);

cleanup:

    LW_SAFE_FREE_STRING(pszDescription);

    return;

error:

    goto cleanup;
}

static
VOID
ADLogMachineTGTRefreshFailureEvent(
    DWORD dwErrCode
    )
{
    DWORD dwError = 0;
    PSTR pszDescription = NULL;
    PSTR pszData = NULL;

    dwError = LwAllocateStringPrintf(
                 &pszDescription,
                 "The Active Directory machine account TGT (Ticket Granting Ticket) failed to refresh.\r\n\r\n" \
                 "     Authentication provider:   %s",
                 LSA_SAFE_LOG_STRING(gpszADProviderName));
    BAIL_ON_LSA_ERROR(dwError);

    dwError = LsaGetErrorMessageForLoggingEvent(
                         dwErrCode,
                         &pszData);

    LsaSrvLogServiceFailureEvent(
            LSASS_EVENT_FAILED_MACHINE_ACCOUNT_TGT_REFRESH,
            KERBEROS_EVENT_CATEGORY,
            pszDescription,
            pszData);

cleanup:

    LW_SAFE_FREE_STRING(pszDescription);
    LW_SAFE_FREE_STRING(pszData);

    return;

error:

    goto cleanup;
}
