#include "net_driver.h"

#ifndef AF_INET
#define AF_INET 2
#endif

NTSTATUS HdNetConnect(PHD_NET_EXTENSION NetExt, PCHAR ServerAddr, USHORT Port, PULONG ConnIdx)
{
    KIRQL oldIrql;
    ULONG i;
    BOOLEAN found = FALSE;

    KeAcquireSpinLock(&NetExt->ConnectionLock, &oldIrql);

    for (i = 0; i < HD_NET_MAX_CONNECTIONS; i++) {
        if (!NetExt->Connections[i].IsActive) {
            NetExt->Connections[i].IsActive = TRUE;
            NetExt->Connections[i].RetryCount = 0;
            NetExt->Connections[i].ServerAddr.sin_family = AF_INET;
            NetExt->Connections[i].ServerAddr.sin_port = RtlUshortByteSwap(Port);
            RtlZeroMemory(&NetExt->Connections[i].ServerAddr.sin_addr, sizeof(IN_ADDR));
            KeInitializeEvent(&NetExt->Connections[i].ConnectEvent, NotificationEvent, FALSE);
            KeInitializeEvent(&NetExt->Connections[i].SendEvent, NotificationEvent, FALSE);
            KeInitializeEvent(&NetExt->Connections[i].RecvEvent, NotificationEvent, FALSE);
            found = TRUE;
            *ConnIdx = i;
            NetExt->ConnectionCount++;
            break;
        }
    }

    KeReleaseSpinLock(&NetExt->ConnectionLock, oldIrql);

    return found ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS HdNetDisconnect(PHD_NET_EXTENSION NetExt, ULONG ConnIdx)
{
    KIRQL oldIrql;

    if (ConnIdx >= HD_NET_MAX_CONNECTIONS) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&NetExt->ConnectionLock, &oldIrql);
    NetExt->Connections[ConnIdx].IsActive = FALSE;
    NetExt->ConnectionCount--;
    KeReleaseSpinLock(&NetExt->ConnectionLock, oldIrql);

    return STATUS_SUCCESS;
}
