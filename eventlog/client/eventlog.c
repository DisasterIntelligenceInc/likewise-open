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
 * Copyright (C) Centeris Corporation 2004-2007
 * Copyright (C) Likewise Software 2007
 * All rights reserved.
 *
 * Authors: Krishna Ganugapati (krishnag@likewisesoftware.com)
 *          Sriram Nambakam (snambakam@likewisesoftware.com)
 *          Kyle Stemen <kstemen@likewise.com>
 *
 * Eventlog Client API
 *
 */
#include "includes.h"

VOID
LwEvtFreeRecord(
    IN PLW_EVENTLOG_RECORD pRecord
    )
{
    LwEvtFreeRecordArray(1, pRecord);
}

VOID
LwEvtFreeRecordArray(
    IN DWORD Count,
    IN PLW_EVENTLOG_RECORD pRecords
    )
{
    DWORD index = 0;

    if (pRecords)
    {
        for (index = 0; index < Count; index++)
        {
            LW_SAFE_FREE_MEMORY(pRecords[index].pLogname);
            LW_SAFE_FREE_MEMORY(pRecords[index].pEventType);
            LW_SAFE_FREE_MEMORY(pRecords[index].pEventSource);
            LW_SAFE_FREE_MEMORY(pRecords[index].pEventCategory);
            LW_SAFE_FREE_MEMORY(pRecords[index].pUser);
            LW_SAFE_FREE_MEMORY(pRecords[index].pComputer);
            LW_SAFE_FREE_MEMORY(pRecords[index].pDescription);
            LW_SAFE_FREE_MEMORY(pRecords[index].pData);
        }

        LW_SAFE_FREE_MEMORY(pRecords);
    }
}

DWORD
LwEvtOpenEventlog(
    IN OPTIONAL PCSTR pServerName,
    OUT PLW_EVENTLOG_CONNECTION* ppConn
    )
{
    volatile DWORD dwError = 0;
    PLW_EVENTLOG_CONNECTION pConn = NULL;
    handle_t eventBindingLocal = 0;

    EVT_LOG_VERBOSE("client::eventlog.c OpenEventlog server=%s)\n",
            LW_SAFE_LOG_STRING(pServerName));

    dwError = LwAllocateMemory(sizeof(*pConn), (PVOID*) &pConn);
    BAIL_ON_EVT_ERROR(dwError);
    
    TRY
    {
        dwError = LwEvtCreateEventlogRpcBinding(pServerName,
                                              &eventBindingLocal);
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    ENDTRY

    BAIL_ON_EVT_ERROR(dwError);

    TRY
    {
        dwError = RpcEvtOpen(eventBindingLocal,
                                &pConn->Handle);
    }
    CATCH (rpc_x_auth_method)
    {
        dwError = ERROR_ACCESS_DENIED;
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    ENDTRY

    BAIL_ON_EVT_ERROR(dwError);

    *ppConn = pConn;

cleanup:
    return dwError;

error:
    if (pConn)
    {
        LwEvtCloseEventlog(pConn);
    }

    if (eventBindingLocal)
    {
        LwEvtFreeEventlogRpcBinding(eventBindingLocal);
    }

    *ppConn = NULL;
    goto cleanup;
}

DWORD
LwEvtCloseEventlog(
    IN PLW_EVENTLOG_CONNECTION pConn
    )
{
    volatile DWORD dwError = 0;

    if (pConn == NULL)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_EVT_ERROR(dwError);
    }

    TRY
    {
        dwError = RpcEvtClose(&pConn->Handle);
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    ENDTRY;

    BAIL_ON_EVT_ERROR(dwError);

cleanup:
    if (pConn)
    {
        if (pConn->Handle != NULL)
        {
            RpcSsDestroyClientContext(&pConn->Handle);
        }
        LW_SAFE_FREE_MEMORY(pConn);
    }

    return dwError;

error:

    EVT_LOG_ERROR("Failed to close event log. Error code [%d]\n", dwError);
    goto cleanup;
}

static
idl_void_p_t
LwEvtRpcAllocateMemory(
    idl_size_t Size
    )
{
    idl_void_p_t pResult = NULL;
    if ((idl_size_t)(DWORD)Size != Size)
    {
        // Overflow
        return NULL;
    }

    if (LwAllocateMemory((DWORD)Size, &pResult))
    {
        // Error
        return NULL;
    }

    return pResult;
}

static
VOID
LwEvtRpcFreeMemory(
    idl_void_p_t pMem
    )
{
    LwFreeMemory(pMem);
}

DWORD
LwEvtReadRecords(
    IN PLW_EVENTLOG_CONNECTION pConn,
    IN DWORD MaxResults,
    IN PCWSTR pSqlFilter,
    OUT PDWORD pCount,
    OUT PLW_EVENTLOG_RECORD* ppRecords
    )
{
    volatile DWORD dwError = 0;
    LW_EVENTLOG_RECORD_LIST records = { 0 };
    // These are function pointers initialized to NULL
    idl_void_p_t (*pOldMalloc)(idl_size_t) = NULL;
    void (*pOldFree)(idl_void_p_t) = NULL;

    TRY
    {
        rpc_ss_swap_client_alloc_free(
                    LwEvtRpcAllocateMemory,
                    LwEvtRpcFreeMemory,
                    &pOldMalloc,
                    &pOldFree);

        dwError = RpcEvtReadRecords(
                    pConn->Handle,
                    MaxResults,
                    (PWSTR)pSqlFilter,
                    &records);
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    FINALLY
    {
        rpc_ss_set_client_alloc_free(
                    pOldMalloc,
                    pOldFree);
    }
    ENDTRY;

    BAIL_ON_EVT_ERROR(dwError);

    *pCount = records.Count;
    *ppRecords = records.pRecords;

cleanup:
    return dwError;

error:
    EVT_LOG_ERROR("Failed to read event log. Error code [%d]\n", dwError);

    LwEvtFreeRecordArray(records.Count, records.pRecords);
    *pCount = 0;
    *ppRecords = NULL;
    goto cleanup;
}


DWORD
LwEvtGetRecordCount(
    IN PLW_EVENTLOG_CONNECTION pConn,
    IN PCWSTR pSqlFilter,
    OUT PDWORD pNumMatched
    )
{
    volatile DWORD dwError = 0;
    DWORD numMatched = 0;

    TRY
    {
        dwError = RpcEvtGetRecordCount(
                    pConn->Handle,
                    (PWSTR)pSqlFilter,
                    &numMatched);
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    ENDTRY;

    BAIL_ON_EVT_ERROR(dwError);

    *pNumMatched = numMatched;

cleanup:
    return dwError;

error:
    EVT_LOG_ERROR("Failed to get record count. Error code [%d]\n", dwError);

    *pNumMatched = 0;
    goto cleanup;
}


DWORD
LwEvtWriteRecords(
    IN PLW_EVENTLOG_CONNECTION pConn,
    IN DWORD Count,
    IN PLW_EVENTLOG_RECORD pRecords
    )
{
    volatile DWORD dwError = 0;
    char pszHostname[1024];
    PWSTR pwszHostname = NULL;
    DWORD index = 0;

    for (index = 0; index < Count; index++)
    {
        if (pRecords[index].pComputer == NULL)
        {
            if (!pwszHostname)
            {
                dwError = LwMapErrnoToLwError(
                            gethostname(pszHostname, sizeof(pszHostname)));
                BAIL_ON_EVT_ERROR(dwError);

                dwError = LwMbsToWc16s(pszHostname, &pwszHostname);
                BAIL_ON_EVT_ERROR(dwError);
            }
            pRecords[index].pComputer = pwszHostname;
        }
    }

    TRY
    {
        dwError = RpcEvtWriteRecords(
                    pConn->Handle,
                    Count,
                    pRecords);
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    ENDTRY;

    BAIL_ON_EVT_ERROR(dwError);

cleanup:
    for (index = 0; index < Count; index++)
    {
        if (pRecords[index].pComputer == pwszHostname)
        {
            pwszHostname = NULL;
        }
    }
    LW_SAFE_FREE_MEMORY(pwszHostname);
    return dwError;

error:
    EVT_LOG_ERROR("Failed to write records. Error code [%d]\n", dwError);

    goto cleanup;
}

DWORD
LwEvtDeleteRecords(
    IN PLW_EVENTLOG_CONNECTION pConn,
    IN OPTIONAL PCWSTR pSqlFilter
    )
{
    volatile DWORD dwError = 0;

    TRY
    {
        dwError = RpcEvtDeleteRecords(
                        pConn->Handle,
                        (PWSTR)pSqlFilter);
    }
    CATCH_ALL
    {
        dwError = EVTGetRpcError(THIS_CATCH);
    }
    ENDTRY;

    BAIL_ON_EVT_ERROR(dwError);

cleanup:
    return dwError;

error:
    EVT_LOG_ERROR("Failed to delete records. Error code [%d]\n", dwError);

    goto cleanup;
}

DWORD
EVTGetRpcError(
    dcethread_exc* exCatch
    )
{
    DWORD dwError = 0;
    dwError = dcethread_exc_getstatus (exCatch);
    return LwNtStatusToWin32Error(LwRpcStatusToNtStatus(dwError));
}

