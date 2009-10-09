/*
 * Copyright (c) Likewise Software.  All rights Reserved.
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
 * Module Name:
 *
 *        executable.c
 *
 * Abstract:
 *
 *        Logic for managing external executable service objects
 *
 * Authors: Brian Koropoff (bkoropoff@likewise.com)
 *
 */

#include "includes.h"

typedef struct _SM_PROCESS_TABLE
{
    pthread_mutex_t lock;
    pthread_mutex_t* pLock;
    SM_LINK execs;
    pthread_t thread;
    BOOLEAN bThreadStarted;
    BOOLEAN bThreadStop;
} SM_PROCESS_TABLE;

typedef struct _SM_EXECUTABLE
{
    LW_SERVICE_STATE state;
    pid_t pid;
    PSM_TABLE_ENTRY pEntry;
    SM_LINK link;
    SM_LINK threadLink;
} SM_EXECUTABLE, *PSM_EXECUTABLE;

static SM_PROCESS_TABLE gProcTable =
{
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .pLock = &gProcTable.lock,
    .execs = {&gProcTable.execs, &gProcTable.execs},
    .thread = (pthread_t) -1,
    .bThreadStarted = FALSE,
    .bThreadStop = FALSE
};

static
DWORD
LwSmExecProgram(
    PSM_EXECUTABLE pExec,
    int* pNotifyPipe
    );

static
PVOID
LwSmExecutableThread(
    PVOID pData
    );

static
DWORD
LwSmExecutableStart(
    PSM_TABLE_ENTRY pEntry
    )
{
    DWORD dwError = 0;
    PSM_EXECUTABLE pExec = pEntry->pData;
    BOOLEAN bLocked = FALSE;
    pid_t pid = -1;
    struct timespec ts = {1, 0};
    int notifyPipe[2] = {-1, -1};
    char c = 0;
    int ret = 0;

    LOCK(bLocked, &gProcTable.lock);

    if (pExec->state != LW_SERVICE_STATE_STOPPED &&
        pExec->state != LW_SERVICE_STATE_DEAD)
    {
        dwError = LW_ERROR_INVALID_SERVICE_TRANSITION;
        BAIL_ON_ERROR(dwError);
    }

    if (pEntry->pInfo->type == LW_SERVICE_TYPE_EXECUTABLE)
    {
        if (pipe(notifyPipe) != 0)
        {
            dwError = LwMapErrnoToLwError(errno);
            BAIL_ON_ERROR(dwError);
        }
    }

    pid = fork();

    if (pid == -1)
    {
        dwError = LwMapErrnoToLwError(errno);
        BAIL_ON_ERROR(dwError);
    }
    else if (pid == 0)
    {
        dwError = LwSmExecProgram(
            pExec,
            pEntry->pInfo->type == LW_SERVICE_TYPE_EXECUTABLE ?
            notifyPipe :
            NULL
            );
        BAIL_ON_ERROR(dwError);
    }
    else
    {
        pExec->pid = pid;
        pExec->state = LW_SERVICE_STATE_STARTING;
        
        /* Take an additional reference to the table entry
           because our child monitoring thread will need it */
        LwSmTableRetainEntry(pEntry);
        
        /* Signal state change */
        LwSmTableNotifyEntryChanged(pEntry);
        
        if (pEntry->pInfo->type == LW_SERVICE_TYPE_EXECUTABLE)
        {
            /* Close write side of pipe since we do not need it */
            close(notifyPipe[1]);
            notifyPipe[1] = -1;

            do
            {
                ret = read(notifyPipe[0], &c, sizeof(c));
            } while (ret < 0 && errno == EINTR);

            if (ret != sizeof(c) || c != 0)
            {
                dwError = LW_ERROR_SERVICE_UNRESPONSIVE;
                BAIL_ON_ERROR(dwError);
            }
        }
        else
        {
            /* Wait for process to start up */
            if (nanosleep(&ts, NULL) != 0)
            {
                dwError = LwMapErrnoToLwError(errno);
                BAIL_ON_ERROR(dwError);
            }
        }

        pExec->state = LW_SERVICE_STATE_RUNNING;

        /* Signal state change */
        LwSmTableNotifyEntryChanged(pEntry);
    }

cleanup:

    if (notifyPipe[0] >= 0)
    {
        close(notifyPipe[0]);
    }

    if (notifyPipe[1] >= 0)
    {
        close(notifyPipe[1]);
    }

    UNLOCK(bLocked, &gProcTable.lock);

    return dwError;

error:

    goto cleanup;
}

static
DWORD
LwSmExecProgram(
    PSM_EXECUTABLE pExec,
    int* pNotifyPipe
    )
{
    DWORD dwError = 0;
    PSM_TABLE_ENTRY pEntry = pExec->pEntry;
    PSTR pszPath = NULL;
    PSTR* ppszArgs = NULL;
    size_t argsLen = 0;
    size_t i = 0;
    sigset_t set;

    sigemptyset(&set);

    dwError = LwMapErrnoToLwError(pthread_sigmask(SIG_SETMASK, &set, NULL));
    BAIL_ON_ERROR(dwError);

    dwError = LwWc16sToMbs(pEntry->pInfo->pwszPath, &pszPath);
    BAIL_ON_ERROR(dwError);

    argsLen = LwSmStringListLength(pEntry->pInfo->ppwszArgs);

    dwError = LwAllocateMemory((argsLen + 1) * sizeof(*ppszArgs), OUT_PPVOID(&ppszArgs));
    BAIL_ON_ERROR(dwError);

    for (i = 0; i < argsLen; i++)
    {
        dwError = LwWc16sToMbs(pEntry->pInfo->ppwszArgs[i], &ppszArgs[i]);
        BAIL_ON_ERROR(dwError);
    }

    if (pNotifyPipe)
    {
        close(pNotifyPipe[0]);
        if (dup2(pNotifyPipe[1], 3) < 0)
        {
            dwError = LwMapErrnoToLwError(errno);
            BAIL_ON_ERROR(dwError);
        }
        close(pNotifyPipe[1]);
        putenv("LIKEWISE_SM_NOTIFY=3");
    }

    if (execv(pszPath, ppszArgs) < 0)
    {
        dwError = LwMapErrnoToLwError(errno);
        BAIL_ON_ERROR(dwError);
    }

    return dwError;

error:

    /* We are in trouble */
    abort();
}

static
DWORD
LwSmExecutableStop(
    PSM_TABLE_ENTRY pEntry
    )
{
    DWORD dwError = 0;
    PSM_EXECUTABLE pExec = pEntry->pData;
    BOOLEAN bLocked = FALSE;

    LOCK(bLocked, &gProcTable.lock);

    switch (pExec->state)
    {
    default:
        dwError = LW_ERROR_INVALID_SERVICE_TRANSITION;
        BAIL_ON_ERROR(dwError);
        break;
    case LW_SERVICE_STATE_RUNNING:
        if (kill(pExec->pid, SIGTERM) < 0)
        {
            dwError = LwMapErrnoToLwError(errno);
            BAIL_ON_ERROR(dwError);
        }
        pExec->state = LW_SERVICE_STATE_STOPPING;
        LwSmTableNotifyEntryChanged(pEntry);
        
        /* The background thread will notice when the
           child process finally exits and update the
           status to LW_SERVICE_STOPPED */
        break;
    case LW_SERVICE_STATE_DEAD:
        /* Go directly to stopped state */
        pExec->state = LW_SERVICE_STATE_STOPPED;
        LwSmTableNotifyEntryChanged(pEntry);
        break;
    }

cleanup:

    UNLOCK(bLocked, &gProcTable.lock);

    return dwError;

error:

    goto cleanup;
}

static
DWORD
LwSmExecutableGetStatus(
    PSM_TABLE_ENTRY pEntry,
    PLW_SERVICE_STATUS pStatus
    )
{
    DWORD dwError = 0;
    PSM_EXECUTABLE pExec = pEntry->pData;
    BOOLEAN bLocked = FALSE;

    LOCK(bLocked, &gProcTable.lock);

    pStatus->state = pExec->state;
    pStatus->pid = pExec->pid;
    pStatus->home = LW_SERVICE_HOME_STANDALONE;

    UNLOCK(bLocked, &gProcTable.lock);

    return dwError;
}

static
DWORD
LwSmExecutableRefresh(
    PSM_TABLE_ENTRY pEntry
    )
{
    DWORD dwError = 0;
    PSM_EXECUTABLE pExec = pEntry->pData;
    BOOLEAN bLocked = FALSE;

    LOCK(bLocked, &gProcTable.lock);

    switch (pExec->state)
    {
    case LW_SERVICE_STATE_RUNNING:
        if (kill(pExec->pid, SIGHUP) < 0)
        {
            dwError = LwMapErrnoToLwError(errno);
            BAIL_ON_ERROR(dwError);
        }
        break;
    default:
        break;
    }

cleanup:

    UNLOCK(bLocked, &gProcTable.lock);

    return dwError;

error:

    goto cleanup;
}

static
DWORD
LwSmExecutableConstruct(
    PSM_TABLE_ENTRY pEntry
    )
{
    DWORD dwError = 0;
    PSM_EXECUTABLE pExec = NULL;
    BOOLEAN bLocked = FALSE;

    dwError = LwAllocateMemory(sizeof(*pExec), OUT_PPVOID(&pExec));
    BAIL_ON_ERROR(dwError);

    pExec->pid = -1;
    pExec->state = LW_SERVICE_STATE_STOPPED;
    pExec->pEntry = pEntry;
    LwSmLinkInit(&pExec->link);
    LwSmLinkInit(&pExec->threadLink);
    pEntry->pData = pExec;

    LOCK(bLocked, &gProcTable.lock);

    LwSmLinkInsertBefore(&gProcTable.execs, &pExec->link);

    if (!gProcTable.bThreadStarted)
    {
        dwError = LwMapErrnoToLwError(pthread_create(
                                          &gProcTable.thread,
                                          NULL,
                                          LwSmExecutableThread,
                                          NULL));
        BAIL_ON_ERROR(dwError);

        gProcTable.bThreadStarted = TRUE;
    }

cleanup:

    UNLOCK(bLocked, &gProcTable.lock);

    return dwError;

error:

    goto cleanup;
}

static
VOID
LwSmExecutableDestruct(
    PSM_TABLE_ENTRY pEntry
    )
{
    return;
}

SM_OBJECT_VTBL gExecutableVtbl =
{
    .pfnStart = LwSmExecutableStart,
    .pfnStop = LwSmExecutableStop,
    .pfnGetStatus = LwSmExecutableGetStatus,
    .pfnRefresh = LwSmExecutableRefresh,
    .pfnConstruct = LwSmExecutableConstruct,
    .pfnDestruct = LwSmExecutableDestruct
};

static
PVOID
LwSmExecutableThread(
    PVOID pData
    )
{
    BOOLEAN bLocked = FALSE;
    PSM_LINK pLink = NULL;
    PSM_LINK pNext = NULL;
    PSM_EXECUTABLE pExec = NULL;
    PSM_TABLE_ENTRY pEntry = NULL;
    pid_t pid = -1;
    int status = 0;
    SM_LINK changed;
    sigset_t set;
    int sig = 0;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    
    LwSmLinkInit(&changed);
    
    for (;;)
    {
        LwSmLinkRemove(&changed);
        
        LOCK(bLocked, &gProcTable.lock);
        
        if (gProcTable.bThreadStop)
        {
            break;
        }
        
        pLink = NULL;

        while ((pLink = SM_LINK_ITERATE(&gProcTable.execs, pLink)))
        {
            pExec = STRUCT_FROM_MEMBER(pLink, SM_EXECUTABLE, link);
            
            if (pExec->pid != -1)
            {
                pid = waitpid(pExec->pid, &status, WNOHANG);
                
                if (pid == pExec->pid)
                {
                    switch (pExec->state)
                    {
                    case LW_SERVICE_STATE_STOPPING:
                        pExec->state = LW_SERVICE_STATE_STOPPED;
                        break;
                    default:
                        pExec->state = LW_SERVICE_STATE_DEAD;
                        break;
                    }
                    
                    pExec->pid = -1;
                    
                    LwSmLinkRemove(&pExec->threadLink);
                    LwSmLinkInsertBefore(&changed, &pExec->threadLink);
                }
            }
        }

        UNLOCK(bLocked, &gProcTable.lock);

        for (pLink = changed.pNext; pLink != &changed; pLink = pNext)
        {
            pNext = pLink->pNext;
            pExec = STRUCT_FROM_MEMBER(pLink, SM_EXECUTABLE, threadLink);
            pEntry = pExec->pEntry;
            
            LwSmLinkRemove(pLink);
            
            LwSmTableNotifyEntryChanged(pEntry);
            LwSmTableReleaseEntry(pEntry);
        }

        do
        {
            sigwait(&set, &sig);
        } while (sig != SIGCHLD);
    }

    return NULL;
}
