#include "net_driver.h"

NTSTATUS HdNetSendHeartbeat(PHD_NET_EXTENSION NetExt, ULONG ConnIdx, ULONGLONG TerminalId)
{
    HD_NET_FRAME_HEADER header = {0};
    ULONG frameId;

    if (ConnIdx >= HD_NET_MAX_CONNECTIONS || !NetExt->Connections[ConnIdx].IsActive) {
        return STATUS_INVALID_PARAMETER;
    }

    frameId = InterlockedIncrement((PLONG)&NetExt->NextFrameId);

    header.Magic      = HD_MAGIC;
    header.Version    = HD_VERSION;
    header.FrameType  = 0x10;
    header.FrameId    = frameId;
    header.ImageId    = TerminalId;
    header.PayloadLen = 0;

    return STATUS_SUCCESS;
}
