#include "net_driver.h"

NTSTATUS HdNetSerializeFrame(PHD_NET_FRAME_HEADER Header, PVOID Payload, PVOID OutFrame, PULONG OutLen)
{
    ULONG totalLen;

    if (!Header || !OutFrame || !OutLen) {
        return STATUS_INVALID_PARAMETER;
    }

    totalLen = HD_NET_FRAME_HDR_SIZE + Header->PayloadLen;
    RtlCopyMemory(OutFrame, Header, HD_NET_FRAME_HDR_SIZE);

    if (Payload && Header->PayloadLen > 0) {
        RtlCopyMemory((PUCHAR)OutFrame + HD_NET_FRAME_HDR_SIZE, Payload, Header->PayloadLen);
    }

    *OutLen = totalLen;
    return STATUS_SUCCESS;
}

NTSTATUS HdNetDeserializeFrame(PVOID Frame, ULONG FrameLen, PHD_NET_FRAME_HEADER Header, PVOID Payload, PULONG PayloadLen)
{
    PHD_NET_FRAME_HEADER pHdr;

    if (!Frame || !Header || FrameLen < HD_NET_FRAME_HDR_SIZE) {
        return STATUS_INVALID_PARAMETER;
    }

    pHdr = (PHD_NET_FRAME_HEADER)Frame;

    if (pHdr->Magic != HD_MAGIC) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    if (pHdr->Version != HD_VERSION) {
        return STATUS_REVISION_MISMATCH;
    }

    RtlCopyMemory(Header, pHdr, HD_NET_FRAME_HDR_SIZE);

    if (Payload && PayloadLen && pHdr->PayloadLen > 0) {
        ULONG copyLen = min(pHdr->PayloadLen, FrameLen - HD_NET_FRAME_HDR_SIZE);
        RtlCopyMemory(Payload, (PUCHAR)Frame + HD_NET_FRAME_HDR_SIZE, copyLen);
        *PayloadLen = copyLen;
    }

    return STATUS_SUCCESS;
}
