#include "includes.h"

NTSTATUS
SrvContextCreate(
    PSMB_SRV_CONNECTION pConnection,
    PSMB_PACKET         pRequest,
    PLWIO_SRV_CONTEXT*  ppContext
    )
{
    NTSTATUS ntStatus = 0;
    PLWIO_SRV_CONTEXT pContext = NULL;

    ntStatus = LW_RTL_ALLOCATE(
                    &pContext,
                    LWIO_SRV_CONTEXT,
                    sizeof(LWIO_SRV_CONTEXT));
    BAIL_ON_NT_STATUS(ntStatus);

    ntStatus = SrvConnectionGetNextSequence(
                    pConnection,
                    pRequest,
                    &pContext->ulRequestSequence);
    BAIL_ON_NT_STATUS(ntStatus);

    if (pConnection->serverProperties.bRequireSecuritySignatures &&
        pConnection->pSessionKey)
    {
        ntStatus = SMBPacketVerifySignature(
                        pRequest,
                        pContext->ulRequestSequence,
                        pConnection->pSessionKey,
                        pConnection->ulSessionKeyLength);
        BAIL_ON_NT_STATUS(ntStatus);
    }

    pContext->pConnection = pConnection;

    InterlockedIncrement(&pConnection->refCount);

    pContext->pRequest = pRequest;

    *ppContext = pContext;

cleanup:

    return ntStatus;

error:

    *ppContext = NULL;

    goto cleanup;
}

VOID
SrvContextFree(
    PVOID pContext
    )
{
    PLWIO_SRV_CONTEXT pIOContext = (PLWIO_SRV_CONTEXT)pContext;

    if (pIOContext->pConnection)
    {
        if (pIOContext->pRequest)
        {
            SMBPacketFree(
                    pIOContext->pConnection->hPacketAllocator,
                    pIOContext->pRequest);
        }

        SrvConnectionRelease(pIOContext->pConnection);
    }

    LwRtlMemoryFree(pIOContext);
}

