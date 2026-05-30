#include "net_driver.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = NULL;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = NULL;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NULL;
    DriverObject->DriverUnload                         = NULL;

    return STATUS_SUCCESS;
}

NTSTATUS HdNetInitialize(PHD_NET_EXTENSION NetExt)
{
    RtlZeroMemory(NetExt, sizeof(HD_NET_EXTENSION));
    KeInitializeSpinLock(&NetExt->ConnectionLock);
    NetExt->NextFrameId = 1;
    return STATUS_SUCCESS;
}

VOID HdNetCleanup(PHD_NET_EXTENSION NetExt)
{
    ULONG i;
    for (i = 0; i < HD_NET_MAX_CONNECTIONS; i++) {
        if (NetExt->Connections[i].IsActive) {
            HdNetDisconnect(NetExt, i);
        }
    }
}
