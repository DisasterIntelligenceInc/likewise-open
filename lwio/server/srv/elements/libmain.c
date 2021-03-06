/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*-
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * Editor Settings: expandtabs and use 4 spaces for indentation */

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
 *        libmain.c
 *
 * Abstract:
 *
 *        Likewise IO (LWIO) - SRV
 *
 *        File Sharing Essential Elements
 *
 *        Library Main
 *
 * Authors: Sriram Nambakam (snambakam@likewise.com)
 *
 */

#include "includes.h"

NTSTATUS
SrvElementsInit(
    VOID
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    int      iIter = 0;

    pthread_mutex_init(&gSrvElements.mutex, NULL);
    gSrvElements.pMutex = &gSrvElements.mutex;

    mt_init_genrand(&gSrvElements.randGen, time(NULL));

    ntStatus = SrvElementsConfigSetupInitial();
    BAIL_ON_NT_STATUS(ntStatus);

    gSrvElements.ulGlobalCreditLimit = SrvElementsConfigGetGlobalCreditLimit();

    ntStatus = SrvElementsResourcesInit();
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = WireGetCurrentNTTime(&gSrvElements.llBootTime);
    BAIL_ON_NT_STATUS(ntStatus);

    while (!RAND_status() && (iIter++ < 10))
    {
        uuid_t uuid;
        CHAR   szUUID[37];

        memset(szUUID, 0, sizeof(szUUID));

        uuid_generate(uuid);
        uuid_unparse(uuid, szUUID);

        RAND_seed(szUUID, sizeof(szUUID));
    }

    ntStatus = SrvTimerInit(&gSrvElements.timer);
    BAIL_ON_NT_STATUS(ntStatus);

    pthread_rwlock_init(&gSrvElements.statsLock, NULL);
    gSrvElements.pStatsLock = &gSrvElements.statsLock;

    ntStatus = SrvAsyncCloseFileTrackerCreate(&gSrvElements.pAsyncCloseFileTracker);
    BAIL_ON_NT_STATUS(ntStatus);

error:

    return ntStatus;
}

NTSTATUS
SrvTimerPostRequest(
    IN  LONG64                 llExpiry,
    IN  PVOID                  pUserData,
    IN  PFN_SRV_TIMER_CALLBACK pfnTimerExpiredCB,
    OUT PSRV_TIMER_REQUEST*    ppTimerRequest
    )
{
    return SrvTimerPostRequestSpecific(
                &gSrvElements.timer,
                llExpiry,
                pUserData,
                pfnTimerExpiredCB,
                ppTimerRequest);
}

NTSTATUS
SrvTimerCancelRequest(
    IN  PSRV_TIMER_REQUEST pTimerRequest,
    PVOID*                 ppUserData
    )
{
    return SrvTimerCancelRequestSpecific(
                &gSrvElements.timer,
                pTimerRequest,
                ppUserData);
}

NTSTATUS
SrvElementsGetBootTime(
    PULONG64 pullBootTime
    )
{
    LONG64   llBootTime = 0LL;
    BOOLEAN  bInLock    = FALSE;

    LWIO_LOCK_MUTEX(bInLock, gSrvElements.pMutex);

    llBootTime = gSrvElements.llBootTime;

    LWIO_UNLOCK_MUTEX(bInLock, gSrvElements.pMutex);

    *pullBootTime = llBootTime;

    return STATUS_SUCCESS;
}

NTSTATUS
SrvElementsGetStats(
    PSRV_ELEMENTS_STATISTICS pStats
    )
{
    BOOLEAN  bInLock  = FALSE;

    LWIO_LOCK_RWMUTEX_SHARED(bInLock, &gSrvElements.statsLock);

    *pStats = gSrvElements.stats;

    LWIO_UNLOCK_RWMUTEX(bInLock, &gSrvElements.statsLock);

    return STATUS_SUCCESS;
}

NTSTATUS
SrvElementsResetStats(
    VOID
    )
{
    BOOLEAN  bInLock  = FALSE;

    LWIO_LOCK_RWMUTEX_EXCLUSIVE(bInLock, &gSrvElements.statsLock);

    gSrvElements.stats.llMaxNumConnections = gSrvElements.stats.llNumConnections;
    gSrvElements.stats.llMaxNumSessions    = gSrvElements.stats.llNumSessions;
    gSrvElements.stats.llMaxNumTreeConnects = gSrvElements.stats.llNumTreeConnects;
    gSrvElements.stats.llMaxNumOpenFiles   = gSrvElements.stats.llNumOpenFiles;

    LWIO_UNLOCK_RWMUTEX(bInLock, &gSrvElements.statsLock);

    return STATUS_SUCCESS;
}

NTSTATUS
SrvElementsShutdown(
    VOID
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    ntStatus = SrvTimerIndicateStop(&gSrvElements.timer);
    BAIL_ON_NT_STATUS(ntStatus);

    SrvTimerFreeContents(&gSrvElements.timer);

    if (gSrvElements.pHintsBuffer != NULL)
    {
        SrvFreeMemory(gSrvElements.pHintsBuffer);
        gSrvElements.pHintsBuffer = NULL;
        gSrvElements.ulHintsLength = 0;
    }

    if (gSrvElements.pStatsLock)
    {
        pthread_rwlock_destroy(&gSrvElements.statsLock);
        gSrvElements.pStatsLock = NULL;
    }

    SrvElementsResourcesShutdown();

    SrvElementsConfigShutdown();

    if (gSrvElements.pMutex)
    {
        pthread_mutex_destroy(&gSrvElements.mutex);
        gSrvElements.pMutex = NULL;
    }

    if (gSrvElements.pAsyncCloseFileTracker)
    {
        SrvAsyncCloseFileTrackerWaitPending(gSrvElements.pAsyncCloseFileTracker);
        SrvAsyncCloseFileTrackerFree(gSrvElements.pAsyncCloseFileTracker);
        gSrvElements.pAsyncCloseFileTracker = NULL;
    }

error:

    return ntStatus;
}

