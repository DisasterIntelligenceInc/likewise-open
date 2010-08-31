/* Editor Settings: expandtabs and use 4 spaces for indentation
 * ex: set softtabstop=4 tabstop=8 expandtab shiftwidth=4: *
 * -*- mode: c, c-basic-offset: 4 -*- */

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

#ifndef __STRUCTS_H__
#define __STRUCTS_H__

#include "lwlist.h"

typedef struct _RDR_OP_CONTEXT
{
    PIRP pIrp;
    SMB_PACKET Packet;
    BOOLEAN (*Continue) (
        struct _RDR_OP_CONTEXT* pContext,
        NTSTATUS status,
        PVOID pParam
        );
    LW_LIST_LINKS Link;
    union
    {
        struct
        {
            LONG64 llByteOffset;
            LONG64 llTotalBytesRead;
            USHORT usReadLen;
        } Read;
        struct
        {
            LONG64 llByteOffset;
            LONG64 llTotalBytesWritten;
        } Write;
        struct
        {
            struct _RDR_CCB* pFile;
            PWSTR pwszFilename;
        } Create;
        struct
        {
            union
            {
                struct _RDR_TREE* pTree;
                struct _RDR_SESSION* pSession;
                struct _RDR_SOCKET* pSocket;
            };
            PSTR pszSharename;
            PIO_CREDS pCreds;
            uid_t Uid;
            PSMB_PACKET pPacket;
            PSTR pszCachePath;
            HANDLE hGssContext;
            struct _RDR_OP_CONTEXT* pContinue;
        } TreeConnect;
    } State;
    USHORT usMid;
} RDR_OP_CONTEXT, *PRDR_OP_CONTEXT;

typedef enum _RDR_SOCKET_STATE
{
    RDR_SOCKET_STATE_NOT_READY,
    RDR_SOCKET_STATE_CONNECTING,
    RDR_SOCKET_STATE_NEGOTIATING,
    RDR_SOCKET_STATE_READY,
    RDR_SOCKET_STATE_ERROR
} RDR_SOCKET_STATE;

typedef struct _RDR_SOCKET
{
    pthread_mutex_t mutex;      /* Locks the structure */

    RDR_SOCKET_STATE volatile state;
    NTSTATUS volatile error;
    int32_t volatile refCount;      /* Reference count */
    BOOLEAN volatile bParentLink;   /* Whether socket is linked to by parent (global socket table) */

    int fd;
    PWSTR pwszHostname;         /* Raw hostname, including channel specifier */
    PWSTR pwszCanonicalName;      /* Canconical hostname for DNS resolution/GSS principal construction */
    struct sockaddr address;    /* For hashing */

    uint32_t maxBufferSize;     /* Max transmit buffer size */
    uint32_t maxRawSize;        /* Maximum raw buffer size */
    uint32_t sessionKey;        /* Socket session key from NEGOTIATE */
    uint32_t capabilities;      /* Remote server capabilities from NEGOTIATE */
    uint8_t *pSecurityBlob;     /* Security blob from NEGOTIATE */
    uint32_t securityBlobLen;   /* Security blob len from NEGOTIATE */

    SMB_HASH_TABLE *pSessionHashByPrincipal;   /* Dependent sessions */
    SMB_HASH_TABLE *pSessionHashByUID;         /* Dependent sessions */

    PLW_TASK pTask;
    PLW_TASK pTimeout;

    BOOLEAN volatile bSessionSetupInProgress;

    uint16_t maxMpxCount;       /* MaxMpxCount from NEGOTIATE */

    SMB_SECURITY_MODE securityMode; /* Share or User security */
    BOOLEAN  bPasswordsMustBeEncrypted;
    BOOLEAN  bSignedMessagesSupported;
    BOOLEAN  bSignedMessagesRequired;
    BOOLEAN  bUseSignedMessagesIfSupported;
    BOOLEAN  bIgnoreServerSignatures;
    BOOLEAN  bShutdown;

    PBYTE    pSessionKey;
    DWORD    dwSessionKeyLength;

    DWORD    dwSequence;
    PSMB_PACKET pPacket; /* Incoming packet */
    PSMB_PACKET pOutgoing; /* Outgoing packet */
    size_t OutgoingWritten;
    LW_LIST_LINKS PendingSend; /* List of RDR_OP_CONTEXTs with packets that need to be sent */
    LW_LIST_LINKS StateWaiters;
    SMB_HASH_TABLE *pResponseHash; /* Storage for dependent responses */
    USHORT usNextMid;
    unsigned volatile bReadBlocked:1;
    unsigned volatile bWriteBlocked:1;
} RDR_SOCKET, *PRDR_SOCKET;

typedef enum _RDR_SESSION_STATE
{
    RDR_SESSION_STATE_NOT_READY,
    RDR_SESSION_STATE_INITIALIZING,
    RDR_SESSION_STATE_READY,
    RDR_SESSION_STATE_ERROR
} RDR_SESSION_STATE;

typedef struct _RDR_SESSION
{
    pthread_mutex_t mutex;      /* Locks the structure */

    RDR_SESSION_STATE volatile state;    /* Session state : valid, invalid, etc */
    NTSTATUS volatile error;
    int32_t volatile refCount;           /* Count of state-change waiters and users */
    BOOLEAN volatile bParentLink;        /* Whether session is linked to by parent (socket) */

    RDR_SOCKET *pSocket;        /* Back pointer to parent socket */

    USHORT uid;

    struct _RDR_SESSION_KEY
    {
        uid_t uid;
        PSTR pszPrincipal;
    } key;

    SMB_HASH_TABLE *pTreeHashByPath;    /* Storage for dependent trees */
    SMB_HASH_TABLE *pTreeHashByTID;     /* Storage for dependent trees */

    PBYTE  pSessionKey;
    DWORD  dwSessionKeyLength;
    PLW_TASK pTimeout;
    LW_LIST_LINKS StateWaiters;
    PRDR_OP_CONTEXT pLogoffContext;
} RDR_SESSION, *PRDR_SESSION;

typedef enum _RDR_TREE_STATE
{
    RDR_TREE_STATE_NOT_READY,
    RDR_TREE_STATE_INITIALIZING,
    RDR_TREE_STATE_READY,
    RDR_TREE_STATE_ERROR
} RDR_TREE_STATE;

typedef struct _RDR_TREE
{
    pthread_mutex_t mutex;      /* Locks both the structure and the hash */
                                /* responses are inserted and removed so often
                                   that a RW lock is probably overkill.*/

    RDR_TREE_STATE volatile state;   /* Tree state: valid, invalid, etc. */
    NTSTATUS volatile error;
    int32_t volatile refCount;           /* Count of state-change waiters and users */
    BOOLEAN volatile bParentLink; /* Whether tree is linked to by parent (session) */

    RDR_SESSION *pSession;      /* Back pointer to parent session */
    uint16_t tid;
    PSTR pszPath;               /* For hashing */
    PLW_TASK pTimeout;
    LW_LIST_LINKS StateWaiters;
    PRDR_OP_CONTEXT pDisconnectContext;
} RDR_TREE, *PRDR_TREE;

typedef struct
{
    USHORT mid;
    PRDR_OP_CONTEXT pContext;
} RDR_RESPONSE, *PRDR_RESPONSE;

typedef struct _RDR_CCB
{
    pthread_mutex_t     mutex;
    pthread_mutex_t*    pMutex;

    PWSTR     pwszPath;

    PRDR_TREE pTree;

    USHORT usFileType;
    uint16_t  fid;
    uint64_t  llOffset;

    struct
    {
        USHORT         usSearchId;
        USHORT         usSearchCount;
        USHORT         usLastNameOffset;
        USHORT         usEndOfSearch;
        SMB_INFO_LEVEL infoLevel;
        PBYTE          pBuffer;
        PBYTE          pCursor;
        ULONG          ulBufferCapacity;
        ULONG          ulBufferLength;
    } find;
} RDR_CCB, *PRDR_CCB;

typedef struct _RDR_CONFIG
{
    ULONG   ulNumMaxPackets;
    BOOLEAN bSignMessagesIfSupported;
} RDR_CONFIG, *PRDR_CONFIG;

typedef struct _RDR_GLOBAL_RUNTIME
{
    RDR_CONFIG        config;
    SMB_HASH_TABLE   *pSocketHashByName;    /* Socket hash by name */
    pthread_mutex_t   socketHashLock;
    pid_t SysPid;
    PLW_THREAD_POOL pThreadPool;
    PLW_TASK_GROUP pReaderTaskGroup;
} RDR_GLOBAL_RUNTIME, *PRDR_GLOBAL_RUNTIME;

#endif

