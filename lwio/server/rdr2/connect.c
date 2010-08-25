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

#include "rdr.h"

static
NTSTATUS
RdrTransceiveTreeConnect(
    PRDR_OP_CONTEXT pContext,
    PRDR_TREE pTree,
    PCWSTR pwszPath
    );

static
NTSTATUS
SMBSrvClientTreeCreate(
    IN OUT PRDR_SESSION* ppSession,
    IN PCSTR pszPath,
    OUT PRDR_TREE* ppTree
    );

static
BOOLEAN
RdrProcessSessionSetupResponse(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    );

static
BOOLEAN
RdrSessionSetupComplete(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    );

static
VOID
RdrNegotiateGssContextWorkItem(
    PVOID pParam
    );

static
NTSTATUS
RdrTransceiveSessionSetup(
    PRDR_OP_CONTEXT pContext,
    PRDR_SESSION pSession,
    PBYTE pBlob,
    DWORD dwBlobLength
    );

static
VOID
RdrFreeTreeConnectContext(
    PRDR_OP_CONTEXT pContext
    )
{
    if (pContext)
    {
        RTL_FREE(&pContext->State.TreeConnect.pszSharename);

        RdrFreePacket(pContext->State.TreeConnect.pPacket);

        if (pContext->State.TreeConnect.pszCachePath)
        {
            SMBKrb5DestroyCache(pContext->State.TreeConnect.pszCachePath);
            RTL_FREE(&pContext->State.TreeConnect.pszCachePath);
        }

        RdrFreeContext(pContext);
    }
}

static
BOOLEAN
RdrFinishTreeConnect(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    )
{
    PRDR_TREE pTree = pContext->State.TreeConnect.pTree;
    PSMB_PACKET pResponsePacket = pParam;
    BOOLEAN bTreeLocked = FALSE;

    LWIO_LOCK_MUTEX(bTreeLocked, &pTree->mutex);

    BAIL_ON_NT_STATUS(status);

    status = pResponsePacket->pSMBHeader->error;
    BAIL_ON_NT_STATUS(status);

    pTree->tid = pResponsePacket->pSMBHeader->tid;
    pTree->state = RDR_TREE_STATE_READY;

cleanup:

    RdrFreePacket(pResponsePacket);

    RdrNotifyContextList(&pTree->StateWaiters, bTreeLocked, &pTree->mutex, status, pTree);

    LWIO_UNLOCK_MUTEX(bTreeLocked, &pTree->mutex);

    RdrFreeTreeConnectContext(pContext);

    return FALSE;

error:

    goto cleanup;
}

static
BOOLEAN
RdrNegotiateComplete(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    )
{
    PRDR_SOCKET pSocket = pParam;
    PRDR_SESSION pSession = NULL;
    BOOLEAN bSessionLocked = FALSE;
    BOOLEAN bFreeContext = FALSE;
    PIO_CREDS pCreds = pContext->State.TreeConnect.pCreds;

    BAIL_ON_NT_STATUS(status);

    status = SMBSrvClientSessionCreate(
        &pSocket,
        pContext->State.TreeConnect.pCreds,
        pContext->State.TreeConnect.Uid,
        &pSession);
    BAIL_ON_NT_STATUS(status);

    pContext->State.TreeConnect.pSession = pSession;

    LWIO_LOCK_MUTEX(bSessionLocked, &pSession->mutex);

    switch (pSession->state)
    {
    case RDR_SESSION_STATE_NOT_READY:
        pSession->state = RDR_SESSION_STATE_INITIALIZING;

        switch (pCreds->type)
        {
        case IO_CREDS_TYPE_KRB5_TGT:
            status = SMBCredTokenToKrb5CredCache(
                pCreds,
                &pContext->State.TreeConnect.pszCachePath);
            BAIL_ON_NT_STATUS(status);
            break;
        case IO_CREDS_TYPE_PLAIN:
            break;
        default:
            status = STATUS_ACCESS_DENIED;
            BAIL_ON_NT_STATUS(status);
        }

        LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);
        RdrProcessSessionSetupResponse(pContext, STATUS_SUCCESS, NULL);
        status = STATUS_PENDING;
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_SESSION_STATE_INITIALIZING:
        LwListInsertTail(&pSession->StateWaiters, &pContext->Link);
        status = STATUS_PENDING;
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_SESSION_STATE_READY:
        LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);
        RdrSessionSetupComplete(pContext, status, pSession);
        status = STATUS_PENDING;
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_SESSION_STATE_ERROR:
        status = pSession->error;
        BAIL_ON_NT_STATUS(status);
        break;
    }

cleanup:

    LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);

    if (status != STATUS_PENDING)
    {
        RdrContinueContext(pContext->State.TreeConnect.pContinue, status, pSession);
        bFreeContext = TRUE;
    }

    if (bFreeContext)
    {
        RdrFreeTreeConnectContext(pContext);
    }

    return FALSE;

error:

    if (status != STATUS_PENDING && pSession)
    {
        LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);
        RdrSessionInvalidate(pSession, status);
        RdrSessionRelease(pSession);
    }

   if (status != STATUS_PENDING && pSocket)
    {
        RdrSocketInvalidate(pSocket, status);
        RdrSocketRelease(pSocket);
    }

    goto cleanup;
}

static
BOOLEAN
RdrProcessNegotiateResponse(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    )
{
    PRDR_SOCKET pSocket = pContext->State.TreeConnect.pSocket;
    PSMB_PACKET pPacket = pParam;
    BOOLEAN bSocketLocked = FALSE;
    BOOLEAN bFreeContext = FALSE;
    PBYTE pGUID = NULL;
    PBYTE pSecurityBlob = NULL;
    DWORD securityBlobLen = 0;
    NEGOTIATE_RESPONSE_HEADER* pHeader = NULL;

    BAIL_ON_NT_STATUS(status);

    LWIO_LOCK_MUTEX(bSocketLocked, &pSocket->mutex);

    status = pPacket->pSMBHeader->error;
    BAIL_ON_NT_STATUS(status);

    status = UnmarshallNegotiateResponse(
        pPacket->pParams,
        pPacket->bufferUsed - (pPacket->pParams - pPacket->pRawBuffer),
        &pHeader,
        &pGUID,
        &pSecurityBlob,
        &securityBlobLen);
    BAIL_ON_NT_STATUS(status);

    // byte order conversions
    SMB_LTOH16_INPLACE(pHeader->dialectIndex);
    SMB_LTOH8_INPLACE(pHeader->securityMode);
    SMB_LTOH16_INPLACE(pHeader->maxMpxCount);
    SMB_LTOH16_INPLACE(pHeader->maxNumberVcs);
    SMB_LTOH32_INPLACE(pHeader->maxBufferSize);
    SMB_LTOH32_INPLACE(pHeader->maxRawSize);
    SMB_LTOH32_INPLACE(pHeader->sessionKey);
    SMB_LTOH32_INPLACE(pHeader->capabilities);
    SMB_LTOH32_INPLACE(pHeader->systemTimeLow);
    SMB_LTOH32_INPLACE(pHeader->systemTimeHigh);
    SMB_LTOH16_INPLACE(pHeader->serverTimeZone);
    SMB_LTOH8_INPLACE(pHeader->encryptionKeyLength);
    SMB_LTOH16_INPLACE(pHeader->byteCount);

    pSocket->maxBufferSize = pHeader->maxBufferSize;
    pSocket->maxRawSize = pHeader->maxRawSize;
    pSocket->sessionKey = pHeader->sessionKey;
    pSocket->capabilities = pHeader->capabilities;
    pSocket->securityMode = (pHeader->securityMode & 0x1) ? SMB_SECURITY_MODE_USER : SMB_SECURITY_MODE_SHARE;
    pSocket->bPasswordsMustBeEncrypted = (pHeader->securityMode & 0x2) ? TRUE : FALSE;
    pSocket->bSignedMessagesSupported = (pHeader->securityMode & 0x4) ? TRUE : FALSE;
    pSocket->bSignedMessagesRequired = (pHeader->securityMode & 0x8) ? TRUE : FALSE;
    pSocket->maxMpxCount = pHeader->maxMpxCount;
    pSocket->securityBlobLen = securityBlobLen;

    status = SMBAllocateMemory(
                    pSocket->securityBlobLen,
                    (PVOID *) &pSocket->pSecurityBlob);
    BAIL_ON_NT_STATUS(status);

    memcpy(pSocket->pSecurityBlob,
           pSecurityBlob,
           pSocket->securityBlobLen);

    pSocket->state = RDR_SOCKET_STATE_READY;

    RdrNotifyContextList(
        &pSocket->StateWaiters,
        bSocketLocked,
        &pSocket->mutex,
        STATUS_SUCCESS,
        pSocket);

    LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);

    RdrNegotiateComplete(pContext, STATUS_SUCCESS, pSocket);
    status = STATUS_PENDING;
    BAIL_ON_NT_STATUS(status);

cleanup:

    LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);

    if (status != STATUS_PENDING)
    {
        RdrContinueContext(pContext->State.TreeConnect.pContinue, status, NULL);
        bFreeContext = TRUE;
    }

    if (bFreeContext)
    {
        RdrFreeTreeConnectContext(pContext);
    }

    return FALSE;

error:

    if (status != STATUS_PENDING && pSocket)
    {
        LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);
        RdrSocketInvalidate(pSocket, status);
        RdrSocketRelease(pSocket);
    }

    /* FIXME: free packet */

    goto cleanup;
}

static
BOOLEAN
RdrProcessSessionSetupResponse(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    )
{
    PRDR_SESSION pSession = pContext->State.TreeConnect.pSession;
    PSMB_PACKET pPacket = pParam;
    BOOLEAN bSessionLocked = FALSE;
    BOOLEAN bFreeContext = FALSE;

    BAIL_ON_NT_STATUS(status);

    LWIO_LOCK_MUTEX(bSessionLocked, &pSession->mutex);

    if (pPacket)
    {
        status = pPacket->pSMBHeader->error;
        if (status == STATUS_MORE_PROCESSING_REQUIRED)
        {
            status = STATUS_SUCCESS;
        }
        BAIL_ON_NT_STATUS(status);

        pSession->uid = pPacket->pSMBHeader->uid;
    }

    /* Save the packet on the context for RdrNegotiateGssContextWorkItem */
    pContext->State.TreeConnect.pPacket = pPacket;
    pPacket = NULL;

    /* Dispatch a work item to negotiate the GSS context in a separate thread.
       Because GSS-API could potentially block in a network call (KRB5)
       or an IPC call (NTLM), calling into it directly from the socket task
       could cause a deadlock. */
    status = LwRtlQueueWorkItem(
        gRdrRuntime.pThreadPool,
        RdrNegotiateGssContextWorkItem,
        pContext,
        0);
    BAIL_ON_NT_STATUS(status);

    status = STATUS_PENDING;
    BAIL_ON_NT_STATUS(status);

cleanup:

    LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);

    if (status != STATUS_PENDING)
    {
        RdrContinueContext(pContext->State.TreeConnect.pContinue, status, NULL);
        bFreeContext = TRUE;
    }

    if (bFreeContext)
    {
        RdrFreeTreeConnectContext(pContext);
    }

    return FALSE;

error:

    if (status != STATUS_PENDING && pSession)
    {
        LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);
        RdrSessionInvalidate(pSession, status);
        RdrSessionRelease(pSession);
    }

    /* FIXME: free packet */

    goto cleanup;
}

static
VOID
RdrNegotiateGssContextWorkItem(
    PVOID pParam
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PRDR_OP_CONTEXT pContext = pParam;
    PRDR_SESSION pSession = pContext->State.TreeConnect.pSession;
    PRDR_SOCKET pSocket = pSession->pSocket;
    PSMB_PACKET pPacket = pContext->State.TreeConnect.pPacket;
    PWSTR pwszNativeOS = NULL;
    PWSTR pwszNativeLanman = NULL;
    PWSTR pwszNativeDomain = NULL;
    PBYTE pInBlob = NULL;
    DWORD dwInBlobLength = 0;
    PBYTE pOutBlob = NULL;
    DWORD dwOutBlobLength = 0;
    PSESSION_SETUP_RESPONSE_HEADER_WC_4 pResponseHeader = NULL;
    BOOLEAN bSessionLocked = FALSE;

    if (pPacket)
    {
        status = UnmarshallSessionSetupResponse_WC_4(
            pPacket->pParams,
            pPacket->bufferLen - pPacket->bufferUsed,
            0,
            &pResponseHeader,
            &pInBlob,
            &pwszNativeOS,
            &pwszNativeLanman,
            &pwszNativeDomain);
        BAIL_ON_NT_STATUS(status);

        dwInBlobLength = pResponseHeader->securityBlobLength;
    }
    else
    {
        pInBlob = pSession->pSocket->pSecurityBlob;
        dwInBlobLength = pSession->pSocket->securityBlobLen;
    }

    if (pContext->State.TreeConnect.pszCachePath)
    {
        status = SMBKrb5SetDefaultCachePath(
            pContext->State.TreeConnect.pszCachePath,
            NULL);
        BAIL_ON_NT_STATUS(status);
    }

    if (!pContext->State.TreeConnect.hGssContext)
    {
        status = SMBGSSContextBuild(
            pSocket->pwszCanonicalName,
            pContext->State.TreeConnect.pCreds,
            &pContext->State.TreeConnect.hGssContext);
        BAIL_ON_NT_STATUS(status);
    }

    status = SMBGSSContextNegotiate(
        pContext->State.TreeConnect.hGssContext,
        pInBlob,
        dwInBlobLength,
        &pOutBlob,
        &dwOutBlobLength);
    BAIL_ON_NT_STATUS(status);

    if (!SMBGSSContextNegotiateComplete(pContext->State.TreeConnect.hGssContext))
    {
        pContext->Continue = RdrProcessSessionSetupResponse;

        status = RdrTransceiveSessionSetup(
            pContext,
            pSession,
            pOutBlob,
            dwOutBlobLength);
        BAIL_ON_NT_STATUS(status);
    }
    else
    {
        LWIO_LOCK_MUTEX(bSessionLocked, &pSession->mutex);

        if (pContext->Packet.haveSignature &&
            (!memcmp(pPacket->pSMBHeader->extra.securitySignature,
                     pContext->Packet.pSMBHeader->extra.securitySignature,
                     sizeof(pContext->Packet.pSMBHeader->extra.securitySignature))))
        {
            LWIO_LOG_WARNING("Server is exhibiting signing bug; ignoring signatures from server");
            RdrSocketSetIgnoreServerSignatures(pSocket, TRUE);
        }

        status = SMBGSSContextGetSessionKey(
            pContext->State.TreeConnect.hGssContext,
            &pSession->pSessionKey,
            &pSession->dwSessionKeyLength);
        BAIL_ON_NT_STATUS(status);

        if (!pSocket->pSessionKey && pSession->pSessionKey)
        {
            status = SMBAllocateMemory(
                pSession->dwSessionKeyLength,
                OUT_PPVOID(&pSocket->pSessionKey));
            BAIL_ON_NT_STATUS(status);

            memcpy(pSocket->pSessionKey, pSession->pSessionKey, pSession->dwSessionKeyLength);
            pSocket->dwSessionKeyLength = pSession->dwSessionKeyLength;
            RdrSocketBeginSequence(pSocket);
        }

        status = SMBSrvClientSocketAddSessionByUID(pSocket, pSession);
        BAIL_ON_NT_STATUS(status);

        pSession->state = RDR_SESSION_STATE_READY;

        RdrNotifyContextList(
            &pSession->StateWaiters,
            bSessionLocked,
            &pSession->mutex,
            status,
            pSession);

        LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);

        RdrSessionSetupComplete(pContext, status, pSession);

        status = STATUS_PENDING;
        BAIL_ON_NT_STATUS(status);
    }

cleanup:

    LWIO_UNLOCK_MUTEX(bSessionLocked, &pSession->mutex);

    RTL_FREE(&pOutBlob);

    if (status != STATUS_PENDING)
    {
        if (pContext->State.TreeConnect.hGssContext)
        {
            SMBGSSContextFree(pContext->State.TreeConnect.hGssContext);
        }

        RdrContinueContext(pContext, status, NULL);
    }

    return;

error:

    goto cleanup;
}

static
BOOLEAN
RdrSessionSetupComplete(
    PRDR_OP_CONTEXT pContext,
    NTSTATUS status,
    PVOID pParam
    )
{
    PRDR_SESSION pSession = pParam;
    PRDR_TREE pTree = NULL;
    BOOLEAN bTreeLocked = FALSE;
    BOOLEAN bFreeContext = FALSE;
    PWSTR pwszPath = NULL;

    BAIL_ON_NT_STATUS(status);

    status = SMBSrvClientTreeCreate(
        &pSession,
        pContext->State.TreeConnect.pszSharename,
        &pTree);
    BAIL_ON_NT_STATUS(status);

    pContext->State.TreeConnect.pTree = pTree;

    LWIO_LOCK_MUTEX(bTreeLocked, &pTree->mutex);

    switch (pTree->state)
    {
    case RDR_TREE_STATE_NOT_READY:
        LwListInsertTail(&pTree->StateWaiters, &pContext->State.TreeConnect.pContinue->Link);
        pTree->state = RDR_TREE_STATE_INITIALIZING;

        status = SMBMbsToWc16s(pTree->pszPath, &pwszPath);
        BAIL_ON_NT_STATUS(status);

        pContext->Continue = RdrFinishTreeConnect;

        status = RdrTransceiveTreeConnect(pContext, pTree, pwszPath);
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_TREE_STATE_INITIALIZING:
        LwListInsertTail(&pTree->StateWaiters, &pContext->State.TreeConnect.pContinue->Link);
        bFreeContext = TRUE;
        status = STATUS_PENDING;
        break;
    case RDR_TREE_STATE_READY:
        bFreeContext = TRUE;
        break;
    case RDR_TREE_STATE_ERROR:
        status = pTree->error;
        BAIL_ON_NT_STATUS(status);
        break;
    }

cleanup:

    LWIO_UNLOCK_MUTEX(bTreeLocked, &pTree->mutex);

    if (status != STATUS_PENDING)
    {
        RdrContinueContext(pContext->State.TreeConnect.pContinue, status, pTree);
        bFreeContext = TRUE;
    }

    if (bFreeContext)
    {
        RdrFreeTreeConnectContext(pContext);
    }

    RTL_FREE(&pwszPath);

    return FALSE;

error:

    if (status != STATUS_PENDING && pTree)
    {
        LWIO_UNLOCK_MUTEX(bTreeLocked, &pTree->mutex);
        RdrTreeInvalidate(pTree, status);
        RdrTreeRelease(pTree);
    }

    goto cleanup;
}

static
NTSTATUS
SMBSrvClientTreeCreate(
    IN OUT PRDR_SESSION* ppSession,
    IN PCSTR pszPath,
    OUT PRDR_TREE* ppTree
    )
{
    DWORD     ntStatus = 0;
    PRDR_TREE pTree = NULL;
    BOOLEAN   bInLock = FALSE;
    PRDR_SESSION pSession = *ppSession;

    LWIO_LOCK_MUTEX(bInLock, &pSession->mutex);

    ntStatus = SMBHashGetValue(
                pSession->pTreeHashByPath,
                pszPath,
                (PVOID *) &pTree);

    if (!ntStatus)
    {
        pTree->refCount++;
        RdrTreeRevive(pTree);
        RdrSessionRelease(pSession);
        *ppSession = NULL;
    }
    else
    {
        ntStatus = RdrTreeCreate(&pTree);
        BAIL_ON_NT_STATUS(ntStatus);

        pTree->pSession = pSession;

        ntStatus = SMBStrndup(
            pszPath,
            strlen(pszPath) + 1,
            &pTree->pszPath);
        BAIL_ON_NT_STATUS(ntStatus);

        ntStatus = SMBHashSetValue(
            pSession->pTreeHashByPath,
            pTree->pszPath,
            pTree);
        BAIL_ON_NT_STATUS(ntStatus);

        pTree->bParentLink = TRUE;

        *ppSession = NULL;
    }

    LWIO_UNLOCK_MUTEX(bInLock, &pSession->mutex);

    *ppTree = pTree;

cleanup:

    return ntStatus;

error:

    LWIO_UNLOCK_MUTEX(bInLock, &pSession->mutex);

    *ppTree = NULL;

    if (pTree)
    {
        RdrTreeRelease(pTree);
    }

    goto cleanup;
}

static
NTSTATUS
RdrTransceiveNegotiate(
    PRDR_OP_CONTEXT pContext,
    PRDR_SOCKET pSocket
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PNEGOTIATE_REQUEST_HEADER pRequestHeader = NULL;
    uint32_t packetByteCount = 0;
    const uchar8_t *pszDialects[1] = { (uchar8_t *) "NT LM 0.12" };

    /* @todo: make initial length configurable */
    status = RdrAllocateContextPacket(pContext, 1024*64);
    BAIL_ON_NT_STATUS(status);

    status = SMBPacketMarshallHeader(
        pContext->Packet.pRawBuffer,
        pContext->Packet.bufferLen,
        COM_NEGOTIATE,
        0, /* error */
        0, /* is response */
        0xFFFF, /* tid */
        gRdrRuntime.SysPid, /* pid */
        0, /* uid */
        0, /* mid */
        FALSE, /* sign messages */
        &pContext->Packet);
    BAIL_ON_NT_STATUS(status);

    pContext->Packet.pData = pContext->Packet.pParams + sizeof(NEGOTIATE_REQUEST_HEADER);
    pContext->Packet.bufferUsed += sizeof(NEGOTIATE_REQUEST_HEADER);
    pContext->Packet.pSMBHeader->wordCount = 0;   /* No parameter words */

    pRequestHeader = (PNEGOTIATE_REQUEST_HEADER)pContext->Packet.pParams;

    status = MarshallNegotiateRequest(
        pContext->Packet.pData,
        pContext->Packet.bufferLen - pContext->Packet.bufferUsed,
        &packetByteCount,
        pszDialects,
        1);
    BAIL_ON_NT_STATUS(status);

    assert(packetByteCount <= UINT16_MAX);
    pRequestHeader->byteCount = (uint16_t) packetByteCount;
    pContext->Packet.bufferUsed += packetByteCount;

    // byte order conversions
    SMB_HTOL16_INPLACE(pRequestHeader->byteCount);

    status = SMBPacketMarshallFooter(&pContext->Packet);
    BAIL_ON_NT_STATUS(status);

    status = RdrSocketTransceive(pSocket, pContext);
    BAIL_ON_NT_STATUS(status);

cleanup:

    return status;

error:

    goto cleanup;
}


static
NTSTATUS
RdrTransceiveTreeConnect(
    PRDR_OP_CONTEXT pContext,
    PRDR_TREE pTree,
    PCWSTR pwszPath
    )
{
    NTSTATUS status = 0;
    uint32_t packetByteCount = 0;
    TREE_CONNECT_REQUEST_HEADER *pHeader = NULL;

    status = RdrAllocateContextPacket(
        pContext,
        1024*64);
    BAIL_ON_NT_STATUS(status);

    status = SMBPacketMarshallHeader(
                    pContext->Packet.pRawBuffer,
                    pContext->Packet.bufferLen,
                    COM_TREE_CONNECT_ANDX,
                    0,
                    0,
                    0,
                    gRdrRuntime.SysPid,
                    pTree->pSession->uid,
                    0,
                    TRUE,
                    &pContext->Packet);
    BAIL_ON_NT_STATUS(status);

    pContext->Packet.pData = pContext->Packet.pParams + sizeof(TREE_CONNECT_REQUEST_HEADER);
    pContext->Packet.bufferUsed += sizeof(TREE_CONNECT_REQUEST_HEADER);
    /* If most commands have word counts which are easy to compute, this
       should be folded into a parameter to SMBPacketMarshallHeader() */
    pContext->Packet.pSMBHeader->wordCount = 4;

    pHeader = (TREE_CONNECT_REQUEST_HEADER *) pContext->Packet.pParams;

    pHeader->flags = 0;
    pHeader->passwordLength = 1;    /* strlen("") + terminating NULL */

    status = MarshallTreeConnectRequestData(
                    pContext->Packet.pData,
                    pContext->Packet.bufferLen - pContext->Packet.bufferUsed,
                    (pContext->Packet.pData - (uint8_t *) pContext->Packet.pSMBHeader) % 2,
                    &packetByteCount,
                    pwszPath,
                    "?????");
    BAIL_ON_NT_STATUS(status);

    assert(packetByteCount <= UINT16_MAX);
    pHeader->byteCount = (uint16_t) packetByteCount;
    pContext->Packet.bufferUsed += packetByteCount;

    // byte order conversions
    SMB_HTOL16_INPLACE(pHeader->flags);
    SMB_HTOL16_INPLACE(pHeader->passwordLength);
    SMB_HTOL16_INPLACE(pHeader->byteCount);

    status = SMBPacketMarshallFooter(&pContext->Packet);
    BAIL_ON_NT_STATUS(status);

    status = RdrSocketTransceive(pTree->pSession->pSocket, pContext);
    BAIL_ON_NT_STATUS(status);

cleanup:

    return status;

error:

    goto cleanup;
}

static
NTSTATUS
RdrTransceiveSessionSetup(
    PRDR_OP_CONTEXT pContext,
    PRDR_SESSION pSession,
    PBYTE pBlob,
    DWORD dwBlobLength
    )
{
    NTSTATUS status = 0;
    uint32_t packetByteCount = 0;
    WCHAR nativeOS[] = {'U','n','i','x','\0'};
    WCHAR nativeLanMan[] = {'L','i','k','e','w','i','s','e',' ','C','I','F','S','\0'};
    WCHAR nativeDomain[] = {'W','O','R','K','G','R','O','U','P','\0'};
    SESSION_SETUP_REQUEST_HEADER_WC_12 *pHeader = NULL;
    PRDR_SOCKET pSocket = pSession->pSocket;

    status = RdrAllocateContextPacket(pContext, 1024*64);
    BAIL_ON_NT_STATUS(status);

    status = SMBPacketMarshallHeader(
                        pContext->Packet.pRawBuffer,
                        pContext->Packet.bufferLen,
                        COM_SESSION_SETUP_ANDX,
                        0,
                        0,
                        0xFFFF,
                        gRdrRuntime.SysPid,
                        pSession->uid,
                        0,
                        TRUE,
                        &pContext->Packet);
    BAIL_ON_NT_STATUS(status);

    pContext->Packet.pData = pContext->Packet.pParams + sizeof(SESSION_SETUP_REQUEST_HEADER_WC_12);
    pContext->Packet.bufferUsed += sizeof(SESSION_SETUP_REQUEST_HEADER_WC_12);
    pContext->Packet.pSMBHeader->wordCount = 12;

    pHeader = (SESSION_SETUP_REQUEST_HEADER_WC_12 *) pContext->Packet.pParams;
    pHeader->maxBufferSize = 12288;
    pHeader->maxMpxCount = 50;
    pHeader->vcNumber = 1;
    pHeader->sessionKey = pSocket->sessionKey;
    pHeader->securityBlobLength = 0;
    pHeader->reserved = 0;
    pHeader->capabilities = CAP_UNICODE | CAP_NT_SMBS | CAP_STATUS32 | CAP_EXTENDED_SECURITY;
    pHeader->securityBlobLength = dwBlobLength;

    status = MarshallSessionSetupRequestData(
        pContext->Packet.pData,
        pContext->Packet.bufferLen - pContext->Packet.bufferUsed,
        (pContext->Packet.pData - (uint8_t *) pContext->Packet.pSMBHeader) % 2,
        &packetByteCount,
        pBlob,
        dwBlobLength,
        nativeOS,
        nativeLanMan,
        nativeDomain);
    BAIL_ON_NT_STATUS(status);

    assert(packetByteCount <= UINT16_MAX);
    pHeader->byteCount = (uint16_t) packetByteCount;
    pContext->Packet.bufferUsed += packetByteCount;

    // byte order conversions
    SMB_HTOL16_INPLACE(pHeader->maxBufferSize);
    SMB_HTOL16_INPLACE(pHeader->maxMpxCount);
    SMB_HTOL16_INPLACE(pHeader->vcNumber);
    SMB_HTOL32_INPLACE(pHeader->sessionKey);
    SMB_HTOL16_INPLACE(pHeader->securityBlobLength);
    //SMB_HTOL32_INPLACE(pHeader->reserved);
    SMB_HTOL32_INPLACE(pHeader->capabilities);
    SMB_HTOL16_INPLACE(pHeader->byteCount);

    status = SMBPacketMarshallFooter(&pContext->Packet);
    BAIL_ON_NT_STATUS(status);

    status = RdrSocketTransceive(pSocket, pContext);
    BAIL_ON_NT_STATUS(status);

cleanup:

    return status;

error:

    goto cleanup;
}

NTSTATUS
RdrTreeConnect(
    PCWSTR pwszHostname,
    PCSTR pszSharename,
    PIO_CREDS pCreds,
    uid_t Uid,
    PRDR_OP_CONTEXT pContinue
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PRDR_OP_CONTEXT pContext = NULL;
    BOOLEAN bSocketLocked = FALSE;
    PRDR_SOCKET pSocket = NULL;

    status = RdrCreateContext(pContinue->pIrp, &pContext);
    BAIL_ON_NT_STATUS(status);

    pContext->State.TreeConnect.Uid = Uid;
    pContext->State.TreeConnect.pContinue = pContinue;

    status = LwRtlCStringDuplicate(
        &pContext->State.TreeConnect.pszSharename,
        pszSharename);
    BAIL_ON_NT_STATUS(status);

    pContext->State.TreeConnect.pCreds = pCreds;

    status = SMBSrvClientSocketCreate(
        pwszHostname,
        &pSocket);
    BAIL_ON_NT_STATUS(status);

    pContext->State.TreeConnect.pSocket = pSocket;

    LWIO_LOCK_MUTEX(bSocketLocked, &pSocket->mutex);

    switch (pSocket->state)
    {
    case RDR_SOCKET_STATE_NOT_READY:
        pSocket->state = RDR_SOCKET_STATE_CONNECTING;
        LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);

        status = RdrSocketConnect(pSocket);
        BAIL_ON_NT_STATUS(status);

        pContext->Continue = RdrProcessNegotiateResponse;

        status = RdrTransceiveNegotiate(pContext, pSocket);
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_SOCKET_STATE_CONNECTING:
    case RDR_SOCKET_STATE_NEGOTIATING:
        LwListInsertTail(&pSocket->StateWaiters, &pContext->Link);
        status = STATUS_PENDING;
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_SOCKET_STATE_READY:
        LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);
        RdrNegotiateComplete(pContext, STATUS_SUCCESS, pSocket);
        status = STATUS_PENDING;
        BAIL_ON_NT_STATUS(status);
        break;
    case RDR_SOCKET_STATE_ERROR:
        status = pSocket->error;
        BAIL_ON_NT_STATUS(status);
        break;
    }

cleanup:

    LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);

    if (status != STATUS_PENDING)
    {
        RdrFreeTreeConnectContext(pContext);
    }

    return status;

error:

    if (status != STATUS_PENDING && pSocket)
    {
        LWIO_UNLOCK_MUTEX(bSocketLocked, &pSocket->mutex);
        RdrSocketInvalidate(pSocket, status);
        RdrSocketRelease(pSocket);
    }

    goto cleanup;
}
