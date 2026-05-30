#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <wsk.h>
#include "hd_driver_ioctl.h"

#define HD_NET_MAX_CONNECTIONS     64
#define HD_NET_DEFAULT_PORT        9527
#define HD_NET_HEARTBEAT_INTERVAL  5000
#define HD_NET_CONNECT_TIMEOUT     10000
#define HD_NET_MAX_RETRY           3
#define HD_NET_FRAME_HDR_SIZE      32

typedef struct _HD_NET_FRAME_HEADER {
    ULONG   Magic;
    USHORT  Version;
    UCHAR   FrameType;
    UCHAR   Flags;
    ULONG   FrameId;
    ULONG   PayloadLen;
    ULONGLONG ImageId;
    ULONGLONG BlockOffset;
    ULONG   BlockCount;
    UCHAR   LayerId;
    UCHAR   Reserved[3];
} HD_NET_FRAME_HEADER, *PHD_NET_FRAME_HEADER;

typedef struct _HD_NET_CONNECTION {
    BOOLEAN         IsActive;
    SOCKADDR_IN     ServerAddr;
    ULONG           RetryCount;
    KEVENT          ConnectEvent;
    KEVENT          SendEvent;
    KEVENT          RecvEvent;
} HD_NET_CONNECTION, *PHD_NET_CONNECTION;

typedef struct _HD_NET_EXTENSION {
    ULONG               ConnectionCount;
    HD_NET_CONNECTION   Connections[HD_NET_MAX_CONNECTIONS];
    KSPIN_LOCK          ConnectionLock;
    ULONG               NextFrameId;
} HD_NET_EXTENSION, *PHD_NET_EXTENSION;

NTSTATUS HdNetInitialize(PHD_NET_EXTENSION NetExt);
VOID     HdNetCleanup(PHD_NET_EXTENSION NetExt);
NTSTATUS HdNetConnect(PHD_NET_EXTENSION NetExt, PCHAR ServerAddr, USHORT Port, PULONG ConnIdx);
NTSTATUS HdNetDisconnect(PHD_NET_EXTENSION NetExt, ULONG ConnIdx);
NTSTATUS HdNetSendBlockRequest(PHD_NET_EXTENSION NetExt, ULONG ConnIdx, PHD_BLOCK_REQUEST Request, PVOID Data, ULONG DataLen);
NTSTATUS HdNetRecvBlockResponse(PHD_NET_EXTENSION NetExt, ULONG ConnIdx, PHD_BLOCK_RESPONSE Response, PVOID Buffer, PULONG BufferLen);
NTSTATUS HdNetSendHeartbeat(PHD_NET_EXTENSION NetExt, ULONG ConnIdx, ULONGLONG TerminalId);
NTSTATUS HdNetSerializeFrame(PHD_NET_FRAME_HEADER Header, PVOID Payload, PVOID OutFrame, PULONG OutLen);
NTSTATUS HdNetDeserializeFrame(PVOID Frame, ULONG FrameLen, PHD_NET_FRAME_HEADER Header, PVOID Payload, PULONG PayloadLen);
