/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 */

/*
 * Copyright (C) Centeris Corporation 2004-2007
 * Copyright (C) Likewise Software    2007-2008
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

/*
 * Abstract: NetAPI connection management functions (rpc client library)
 *
 * Authors: Rafal Szczesniak (rafal@likewisesoftware.com)
 */

#include "includes.h"


NTSTATUS
NetConnListInit(
    void
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    int locked = 0;
    NetConnList *list = NULL;

    GLOBAL_DATA_LOCK(locked);

    /* Don't try to intialise what's already been initialised */
    if (conn_list) goto cleanup;

    status = NetAllocateMemory((void**)&list, sizeof(NetConnList), NULL);
    BAIL_ON_NTSTATUS_ERROR(status);

    list->conn = NULL;

    /* According to pthread_mutex_init(3) documentation
       the function call always succeeds */
    pthread_mutex_init(&list->mutex, NULL);

    conn_list = list;

cleanup:
    GLOBAL_DATA_UNLOCK(locked);

    return status;

error:
    if (list) {
        NetFreeMemory((void*)list);
    }

    conn_list = NULL;
    goto cleanup;
}


NTSTATUS
NetConnListDestroy(
    void
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    int locked = 0;
    NetConnList *list = NULL;
    int ret = 0;

    GLOBAL_DATA_LOCK(locked);

    list = conn_list;

    status = NetDisconnectAll();
    BAIL_ON_NTSTATUS_ERROR(status);

    ret = pthread_mutex_destroy(&list->mutex);
    if (ret) {
        status = STATUS_UNSUCCESSFUL;
        goto error;
    }

    if (list) {
        NetFreeMemory((void*)list);
    }

    conn_list = NULL;

cleanup:
    GLOBAL_DATA_UNLOCK(locked);

    return status;

error:
    goto cleanup;
}


NTSTATUS
NetConnAdd(
    NetConn *c
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    NetConn *last = NULL;
    int locked = 0;

    BAIL_ON_INVALID_PTR(c);

    CONN_LIST_LOCK(conn_list);

    /* Find the last connection */
    last = conn_list->conn;
    while (last && last->next) last = last->next;

    if (last == NULL) {
        /* This is the very first connection */
        conn_list->conn = c;

    } else {
        /* Append the new connection */
        last->next = c;
    }

    c->next = NULL;

done:
    CONN_LIST_UNLOCK(conn_list);
    return status;

error:
    goto done;
}


NTSTATUS
NetConnDelete(
    NetConn *c
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    NetConn *prev = NULL;
    int locked = 0;

    BAIL_ON_INVALID_PTR(c);

    CONN_LIST_LOCK(conn_list);

    /* Simple case - this happens to be the first connection */
    if (c == conn_list->conn) {
        conn_list->conn = c->next;
        goto done;
    }

    /* Find connection that is previous to the requested one */
    prev = conn_list->conn;
    while (prev && prev->next != c) {
        prev = prev->next;
    }

    if (prev == NULL) {
        status = STATUS_INVALID_PARAMETER;
        goto error;
    }

    prev->next = c->next;

done:
    CONN_LIST_UNLOCK(conn_list);
    return status;

error:
    goto done;
}


NetConn*
FindNetConn(
    const wchar16_t *name
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    int locked = 0;
    NetConn *c = NULL;

    BAIL_ON_INVALID_PTR(name);

    CONN_LIST_LOCK(conn_list);

    c = conn_list->conn;
    while (c && wc16scmp(c->hostname, name)) {
        c = c->next;
    }

cleanup:
    CONN_LIST_UNLOCK(conn_list);

    return c;

error:
    c = NULL;
    goto cleanup;
}


NTSTATUS
NetConnectSamr(
    NetConn **conn,
    const wchar16_t *hostname,
    uint32 req_dom_flags,
    uint32 req_btin_dom_flags,
    PIO_CREDS creds
    )
{
    const uint32 conn_flags = SAMR_ACCESS_OPEN_DOMAIN |
                              SAMR_ACCESS_ENUM_DOMAINS;

    const uint32 dom_flags = DOMAIN_ACCESS_ENUM_ACCOUNTS |
                             DOMAIN_ACCESS_OPEN_ACCOUNT |
                             DOMAIN_ACCESS_LOOKUP_INFO_2;

    const uint32 btin_dom_flags = DOMAIN_ACCESS_ENUM_ACCOUNTS |
                                  DOMAIN_ACCESS_OPEN_ACCOUNT |
                                  DOMAIN_ACCESS_LOOKUP_INFO_2;
    const uint32 size = 128;
    const char *builtin = "BUILTIN";
    const char *localhost = "127.0.0.1";

    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    RPCSTATUS rpcstatus = 0;
    handle_t samr_b = NULL;
    NetConnList *cnlist = NULL;
    NetConn *cn = NULL;
    NetConn *lookup = NULL;
    PolicyHandle conn_handle = {0};
    PolicyHandle dom_handle = {0};
    PolicyHandle btin_dom_handle = {0};
    PSID btin_dom_sid = NULL;
    PSID dom_sid = NULL;
    uint32 conn_access = 0;
    uint32 dom_access = 0;
    uint32 btin_dom_access = 0;
    uint32 resume = 0;
    uint32 entries = 0;
    uint32 i = 0;
    char *host = NULL;
    wchar16_t **dom_names = NULL;
    wchar16_t *dom_name = NULL;
    wchar16_t localhost_addr[10];
    rpc_transport_info_handle_t trans_info = NULL;
    unsigned char *sess_key = NULL;
    unsigned16 sess_key_len = 0;

    BAIL_ON_INVALID_PTR(conn);

    status = NetConnListInit();
    BAIL_ON_NTSTATUS_ERROR(status);

    cnlist = conn_list;

    /* check if requested connection is a localhost connection */
    if (hostname == NULL) {
        size_t addr_size = sizeof(localhost_addr)/sizeof(wchar16_t);
        memset(localhost_addr, 0, addr_size);
        mbstowc16s(localhost_addr, localhost, addr_size);
        hostname = (wchar16_t*)localhost_addr;
    }

    /* look for existing connection and check if it already has
       required access rights */
    lookup = FindNetConn(hostname);
    if (lookup &&
        (req_dom_flags == 0 ||
         ((lookup->samr.dom_access & req_dom_flags) == req_dom_flags)) &&
        (req_btin_dom_flags == 0 ||
         ((lookup->samr.btin_dom_access & req_btin_dom_flags) == req_btin_dom_flags))) {

        *conn = lookup;
        goto cleanup;
    }

    if (lookup == NULL) {
        /* create a new connection */
        status = NetAllocateMemory((void**)&cn, sizeof(NetConn), cnlist);
        BAIL_ON_NTSTATUS_ERROR(status);

    } else {
        cn = lookup;
    }

    if (cn->samr.conn_access == 0 ||
        cn->samr.bind == NULL) {
        conn_access = conn_flags;

        host = awc16stombs(hostname);
        BAIL_ON_NO_MEMORY(host);

        rpcstatus = InitSamrBindingDefault(&samr_b, host, creds);
        if (rpcstatus != 0) {
            BAIL_ON_NTSTATUS_ERROR(STATUS_UNSUCCESSFUL);
        }

        status = SamrConnect2(samr_b, hostname, conn_access, &conn_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        cn->samr.conn_access = conn_access;
        cn->samr.bind        = samr_b;
        cn->samr.conn_handle = conn_handle;

        sess_key     = NULL;
        sess_key_len = 0;

        rpc_binding_inq_transport_info(samr_b, &trans_info, &rpcstatus);
        if (rpcstatus)
        {
            BAIL_ON_NTSTATUS_ERROR(STATUS_CONNECTION_INVALID);
        }

        rpc_smb_transport_info_inq_session_key(trans_info, &sess_key,
                                               &sess_key_len);
        if (sess_key_len > 0)
        {
            memcpy((void*)cn->sess_key, sess_key, sizeof(cn->sess_key));
            cn->sess_key_len = (uint32)sess_key_len;
        }

    } else {
        samr_b      = cn->samr.bind;
        conn_handle = cn->samr.conn_handle;
    }

    /* check if requested builtin domain access flags have been
       specified and whether they match already opened handle's
       access rights */
    if (req_btin_dom_flags != 0 &&
        cn->samr.btin_dom_access != 0 &&
        (cn->samr.btin_dom_access & req_btin_dom_flags) != req_btin_dom_flags) {

        status = SamrClose(samr_b, &cn->samr.btin_dom_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        memset(&cn->samr.btin_dom_handle, 0, sizeof(cn->samr.btin_dom_handle));
        cn->samr.btin_dom_access = 0;
    }

    if (cn->samr.btin_dom_access == 0) {
        btin_dom_access = btin_dom_flags | req_btin_dom_flags;
        conn_handle = cn->samr.conn_handle;

        status = RtlAllocateSidFromCString(&btin_dom_sid, SID_BUILTIN_DOMAIN);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = SamrOpenDomain(samr_b, &conn_handle, btin_dom_access,
                                btin_dom_sid, &btin_dom_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        cn->samr.btin_dom_handle = btin_dom_handle;
        cn->samr.btin_dom_access = btin_dom_access;

        RTL_FREE(&btin_dom_sid);
    }

    /* check if requested host's domain access flags have been
       specified and whether they match already opened handle's
       access rights */
    if (req_dom_flags != 0 &&
        cn->samr.dom_access != 0 &&
        (cn->samr.dom_access & req_dom_flags) != req_dom_flags) {

        status = SamrClose(samr_b, &cn->samr.dom_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        memset(&cn->samr.dom_handle, 0, sizeof(cn->samr.dom_handle));
        cn->samr.dom_access = 0;
        if (cn->samr.dom_sid) {
            RTL_FREE(&cn->samr.dom_sid);
            cn->samr.dom_sid = NULL;
        }
    }

    if (cn->samr.dom_access == 0) {
        resume    = 0;
        entries   = 0;
        dom_names = NULL;

        do {
            status = SamrEnumDomains(samr_b, &conn_handle, &resume, size,
                                     &dom_names, &entries);
            if (status != STATUS_SUCCESS &&
                status != STATUS_MORE_ENTRIES) goto error;

            for (i = 0; i < entries; i++) {
                char n[32]; /* any netbios name can fit here */

                wc16stombs(n, dom_names[i], sizeof(n));

                /* pick up first domain name that is not a builtin domain */
                if (strcasecmp(n, builtin)) {
                    dom_name = wc16sdup(dom_names[i]);
                    BAIL_ON_NO_MEMORY(dom_name);

                    SamrFreeMemory((void*)dom_names);
                    dom_names = NULL;
                    goto domain_name_found;
                }
            }

            if (dom_names) {
                SamrFreeMemory((void*)dom_names);
                dom_names = NULL;
            }

        } while (status == STATUS_MORE_ENTRIES);

domain_name_found:
        status = SamrLookupDomain(samr_b, &conn_handle, dom_name, &dom_sid);
        BAIL_ON_NTSTATUS_ERROR(status);

        dom_access = dom_flags | req_dom_flags;

        status = SamrOpenDomain(samr_b, &conn_handle, dom_access, dom_sid,
                                &dom_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        cn->samr.dom_handle = dom_handle;
        cn->samr.dom_access = dom_access;
        cn->samr.dom_name   = wc16sdup(dom_name);

        status = NetAddDepMemory(cn->samr.dom_name, cn);
        BAIL_ON_NTSTATUS_ERROR(status);

        if (dom_sid) {
            MsRpcDuplicateSid(&cn->samr.dom_sid, dom_sid);
            BAIL_ON_NTSTATUS_ERROR(status);

        } else {
            status = STATUS_INVALID_SID;
            goto error;
        }
    }

    /* set the host name if it's completely new connection */
    if (cn->hostname == NULL) {
        cn->hostname = wc16sdup(hostname);
        BAIL_ON_NO_MEMORY(cn->hostname);

        status = NetAddDepMemory(cn->hostname, cn);
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    /* add newly created connection */
    if (cn != lookup) {
        status = NetConnAdd(cn);
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    /* return initialised connection and status code */
    *conn = cn;

cleanup:
    if (btin_dom_sid) {
        RTL_FREE(&btin_dom_sid);
    }

    if (dom_sid) {
        SamrFreeMemory((void*)dom_sid);
    }

    if (dom_names) {
        SamrFreeMemory((void*)dom_names);
    }

    SAFE_FREE(host);
    SAFE_FREE(dom_name);

    return status;

error:
    if (cn) {
        /* an error occured somewhere so we don't want to overwrite
           status code with result of this call */
        NetDisconnectSamr(cn);
    }

    *conn = NULL;
    goto cleanup;
}


NTSTATUS
NetConnectLsa(
    NetConn **conn,
    const wchar16_t *hostname,
    uint32 req_lsa_flags,
    PIO_CREDS creds
    )
{
    const uint32 lsa_flags = LSA_ACCESS_LOOKUP_NAMES_SIDS;
    const char *localhost = "127.0.0.1";

    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    RPCSTATUS rpcstatus = 0;
    char *host = NULL;
    handle_t lsa_b = NULL;
    NetConnList *cnlist = NULL;
    NetConn *cn = NULL;
    NetConn *lookup = NULL;
    PolicyHandle policy_handle = {0};
    uint32 lsa_access = 0;
    wchar16_t localhost_addr[10] = {0};

    BAIL_ON_INVALID_PTR(conn);

    status = NetConnListInit();
    BAIL_ON_NTSTATUS_ERROR(status);

    cnlist = conn_list;

    /* check if requested connection is a localhost connection */
    if (hostname == NULL) {
        size_t addr_size = sizeof(localhost_addr)/sizeof(wchar16_t);
        memset(localhost_addr, 0, addr_size);
        mbstowc16s(localhost_addr, localhost, addr_size);
        hostname = (wchar16_t*)localhost_addr;
    }

    /* look for existing connection */
    lookup = FindNetConn(hostname);
    if (lookup &&
        (req_lsa_flags == 0 || (lookup->lsa.lsa_access & req_lsa_flags))) {

        *conn = lookup;
        goto cleanup;
    }

    if (lookup == NULL) {
        /* create a new connection */
        status = NetAllocateMemory((void**)&cn, sizeof(NetConn), cnlist);
        BAIL_ON_NTSTATUS_ERROR(status);

    } else {
        cn = lookup;
    }

    if (!(cn->lsa.lsa_access & req_lsa_flags) &&
        cn->lsa.bind) {
        status = LsaClose(lsa_b, &cn->lsa.policy_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        memset(&cn->lsa.policy_handle, 0, sizeof(cn->lsa.policy_handle));
        cn->lsa.lsa_access = 0;
    }

    if (cn->lsa.lsa_access == 0 ||
        cn->lsa.bind == NULL) {
        lsa_access = lsa_flags | req_lsa_flags;

        host = awc16stombs(hostname);
        BAIL_ON_NO_MEMORY(host);

        rpcstatus = InitLsaBindingDefault(&lsa_b, host, creds);
        if (rpcstatus != 0) {
            status = STATUS_UNSUCCESSFUL;
            goto error;
        }

        status = LsaOpenPolicy2(lsa_b, hostname, NULL, lsa_access,
                                &policy_handle);
        BAIL_ON_NTSTATUS_ERROR(status);

        cn->lsa.bind          = lsa_b;
        cn->lsa.policy_handle = policy_handle;
        cn->lsa.lsa_access    = lsa_access;
    }

    /* set the host name if it's completely new connection */
    if (cn->hostname == NULL) {
        cn->hostname = wc16sdup(hostname);
        BAIL_ON_NO_MEMORY(cn->hostname);

        status = NetAddDepMemory(cn->hostname, cn);
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    /* add newly created connection (if it is in fact new) */
    if (cn != lookup) {
        status = NetConnAdd(cn);
        BAIL_ON_NTSTATUS_ERROR(status);
    }

    /* return initialised connection and status code */
    *conn = cn;

cleanup:
    SAFE_FREE(host);

    return status;

error:
    if (cn) {
        /* an error occured somewhere so we don't want to overwrite
           status code with result of this call */
        NetDisconnectLsa(cn);
    }

    *conn = NULL;
    goto cleanup;
}


NTSTATUS
NetDisconnectSamr(
    NetConn *cn
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    handle_t samr_b = NULL;

    BAIL_ON_INVALID_PTR(cn);

    samr_b = cn->samr.bind;

    status = SamrClose(samr_b, &cn->samr.dom_handle);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(&cn->samr.dom_handle, 0, sizeof(cn->samr.dom_handle));
    cn->samr.dom_access = 0;
    if (cn->samr.dom_name) {
        NetFreeMemory((void*)cn->samr.dom_name);

    }

    if (cn->samr.dom_sid) {
        RTL_FREE(&cn->samr.dom_sid);
    }

    status = SamrClose(samr_b, &cn->samr.btin_dom_handle);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(&cn->samr.btin_dom_handle, 0, sizeof(cn->samr.btin_dom_handle));
    cn->samr.btin_dom_access = 0;

    status = SamrClose(samr_b, &cn->samr.conn_handle);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(&cn->samr.conn_handle, 0, sizeof(cn->samr.conn_handle));
    cn->samr.conn_access = 0;

    FreeSamrBinding(&samr_b);
    cn->samr.bind = NULL;

    if (cn->lsa.bind == NULL) {
        status = NetConnDelete(cn);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = NetFreeMemory((void*)cn);
        BAIL_ON_NTSTATUS_ERROR(status);

        cn = NULL;
    }

cleanup:
    return status;

error:
    goto cleanup;
}


NTSTATUS
NetDisconnectLsa(
    NetConn *cn
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    handle_t lsa_b = NULL;

    BAIL_ON_INVALID_PTR(cn);

    lsa_b = cn->lsa.bind;

    status = LsaClose(lsa_b, &cn->lsa.policy_handle);
    BAIL_ON_NTSTATUS_ERROR(status);

    memset(&cn->lsa.policy_handle, 0, sizeof(cn->lsa.policy_handle));
    cn->lsa.lsa_access = 0;

    FreeLsaBinding(&lsa_b);
    cn->lsa.bind = NULL;

    if (cn->samr.bind == NULL) {
        status = NetConnDelete(cn);
        BAIL_ON_NTSTATUS_ERROR(status);

        status = NetFreeMemory((void*)cn);
        BAIL_ON_NTSTATUS_ERROR(status);

        cn = NULL;
    }

cleanup:
    return status;

error:
    goto cleanup;
}


NTSTATUS
NetDisconnectAll(
    void
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WINERR err = ERROR_SUCCESS;
    NetConn *cn = NULL;

    /* close any remaining connections */
    cn = conn_list->conn;
    while (cn) {
        status = NetDisconnectSamr(cn);
        BAIL_ON_NTSTATUS_ERROR(status);

        if (cn) {
            status = NetDisconnectLsa(cn);
            BAIL_ON_NTSTATUS_ERROR(status);
        }

        cn = conn_list->conn;
    }

cleanup:
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
