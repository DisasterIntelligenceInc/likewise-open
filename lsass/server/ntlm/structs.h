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
 * Copyright (C) Likewise Software. All rights reserved.
 *
 * Module Name:
 *
 *        structs.h
 *
 * Abstract:
 *
 *
 * Authors: Marc Guy (mguy@likewisesoftware.com)
 *
 */

#ifndef __STRUCTS_H__
#define __STRUCTS_H__

// We're only going to list the non-optional members of these structures so that
// optional members will just hang off the end
typedef struct _NTLM_NEGOTIATE_MESSAGE
{
    UCHAR NtlmSignature[NTLM_NETWORK_SIGNATURE_SIZE];
    DWORD MessageType;
    DWORD NtlmFlags;
    // Optional Supplied Domain NTLM_SEC_BUFFER
    // Optional Supplied Workstation NTLM_SEC_BUFFER
    // Optional OS Version 8 bytes
    // Optional Data
} NTLM_NEGOTIATE_MESSAGE, *PNTLM_NEGOTIATE_MESSAGE;

typedef struct _NTLM_CHALLENGE_MESSAGE
{
    UCHAR NtlmSignature[NTLM_NETWORK_SIGNATURE_SIZE];
    DWORD MessageType;
    NTLM_SEC_BUFFER Target;
    DWORD NtlmFlags;
    UCHAR Challenge[NTLM_CHALLENGE_SIZE];
    // Optional Context 8 bytes
    // Optional Target Information NTLM_SEC_BUFFER
    // Optional OS Version 8 bytes
    // Optional Data
} NTLM_CHALLENGE_MESSAGE, *PNTLM_CHALLENGE_MESSAGE;

typedef struct _NTLM_RESPONSE_MESSAGE
{
    UCHAR NtlmSignature[NTLM_NETWORK_SIGNATURE_SIZE];
    DWORD MessageType;
    NTLM_SEC_BUFFER LmResponse;
    NTLM_SEC_BUFFER NtResponse;
    NTLM_SEC_BUFFER AuthTargetName;
    NTLM_SEC_BUFFER UserName;
    NTLM_SEC_BUFFER Workstation;
    // Optional Session Key NTLM_SEC_BUFFER
    // Optional Flags 4 bytes
    // Optional OS Version 8 bytes
    // Optional Data
} NTLM_RESPONSE_MESSAGE, *PNTLM_RESPONSE_MESSAGE;

typedef struct _NTLM_BLOB
{
    BYTE NtlmBlobSignature[4];
    DWORD Reserved1;
    ULONG64 TimeStamp;
    ULONG64 ClientNonce;
    DWORD Reserved2;
    // Target information block
    // DWORD Reserved3
} NTLM_BLOB, *PNTLM_BLOB;

typedef struct _NTLM_TARGET_INFO_BLOCK
{
    SHORT sType;
    SHORT sLength;
    //BYTE  Contents[0];
} NTLM_TARGET_INFO_BLOCK, *PNTLM_TARGET_INFO_BLOCK;

typedef enum
{
    NtlmStateBlank,
    NtlmStateNegotiate,
    NtlmStateChallenge,
    NtlmStateResponse
} NTLM_STATE, *PNTLM_STATE;

typedef struct _NTLM_CONTEXT
{
    NTLM_STATE NtlmState;
    DWORD dwMessageSize;
    PVOID pMessage;
    NTLM_CRED_HANDLE CredHandle;
    DWORD NegotiatedFlags;
    LONG nRefCount;
    BYTE SessionKey[NTLM_SESSION_KEY_SIZE];
    DWORD cbSessionKeyLen;
    RC4_KEY* pSignKey;
    RC4_KEY* pSealKey;
    RC4_KEY* pVerifyKey;
    RC4_KEY* pUnsealKey;
    DWORD dwMsgSeqNum;
} NTLM_CONTEXT, *PNTLM_CONTEXT;

typedef struct _NTLM_CREDENTIALS
{
    LSA_CRED_HANDLE CredHandle;
    DWORD dwCredDirection;
    LONG nRefCount;
} NTLM_CREDENTIALS, *PNTLM_CREDENTIALS;

#endif /* __STRUCTS_H__ */
