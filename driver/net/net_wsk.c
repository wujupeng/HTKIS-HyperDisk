#include "net_driver.h"

NTSTATUS HdNetSendBlockRequest(PHD_NET_EXTENSION NetExt, ULONG ConnIdx, PHD_BLOCK_REQUEST Request, PVOID Data, ULONG DataLen)
{
    HD_NET_FRAME_HEADER header = {0};
    ULONG frameId;

    if (ConnIdx >= HD_NET_MAX_CONNECTIONS || !NetExt->Connections[ConnIdx].IsActive) {
        return STATUS_INVALID_PARAMETER;
    }

    frameId = InterlockedIncrement((PLONG)&NetExt->NextFrameId);

    header.Magic       = HD_MAGIC;
    header.Version     = HD_VERSION;
    header.FrameType   = Request->IsWrite ? 0x03 : 0x01;
    header.FrameId     = frameId;
    header.ImageId     = Request->ImageId;
    header.BlockOffset = Request->BlockOffset;
    header.BlockCount  = Request->BlockCount;
    header.LayerId     = Request->LayerId;
    header.PayloadLen  = DataLen;

    UNREFERENCED_PARAMETER(Data);

    return STATUS_SUCCESS;
}

NTSTATUS HdNetRecvBlockResponse(PHD_NET_EXTENSION NetExt, ULONG ConnIdx, PHD_BLOCK_RESPONSE Response, PVOID Buffer, PULONG BufferLen)
{
    if (ConnIdx >= HD_NET_MAX_CONNECTIONS || !NetExt->Connections[ConnIdx].IsActive) {
        return STATUS_INVALID_PARAMETER;
    }

    UNREFERENCED_PARAMETER(Response);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(BufferLen);

    return STATUS_SUCCESS;
}
