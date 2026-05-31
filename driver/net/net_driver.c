#include "net_driver.h"
#include "../common/hd_serial.h"

HD_SERIAL_DEBUG g_SerialDebug;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symLinkName;
    PDEVICE_OBJECT deviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    HdSerialInitialize(&g_SerialDebug);
    HdSerialWriteString(&g_SerialDebug, "[HDx:NET_LOADED] HDNet DriverEntry\r\n");

    RtlInitUnicodeString(&deviceName, L"\\Device\\HyperDiskNet");
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\HyperDiskNet");

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_NETWORK, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        HdSerialWriteString(&g_SerialDebug, "[HDx:NET_LOADED] IoCreateDevice failed\r\n");
        return status;
    }

    status = IoCreateSymbolicLink(&symLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = NULL;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = NULL;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NULL;
    DriverObject->DriverUnload                         = NULL;

    HdSerialWriteString(&g_SerialDebug, "[HDx:NET_LOADED] HDNet device created, ready for HDBlk\r\n");

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
