#include "bus_driver.h"

PHD_BUS_EXTENSION g_BusExtension = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symLinkName;
    PDEVICE_OBJECT deviceObject = NULL;
    PHD_BUS_EXTENSION busExt = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&deviceName, HD_BUS_DEVICE_NAME);
    RtlInitUnicodeString(&symLinkName, HD_BUS_SYM_LINK);

    status = IoCreateDevice(
        DriverObject,
        sizeof(HD_BUS_EXTENSION),
        &deviceName,
        FILE_DEVICE_BUS_EXTENDER,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    busExt = (PHD_BUS_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(busExt, sizeof(HD_BUS_EXTENSION));
    busExt->BusDeviceObject = deviceObject;
    busExt->BusDeviceName = deviceName;
    busExt->BusSymLinkName = symLinkName;
    busExt->ChildCount = 0;

    InitializeListHead(&busExt->ChildListHead);
    KeInitializeSpinLock(&busExt->ChildListLock);

    status = IoCreateSymbolicLink(&symLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->DriverExtension->AddDevice = NULL;
    DriverObject->MajorFunction[IRP_MJ_CREATE]         = HdBusDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = HdBusDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HdBusDispatchIoctl;
    DriverObject->MajorFunction[IRP_MJ_PNP]            = HdBusDispatchPnp;
    DriverObject->DriverUnload                         = HdBusUnload;

    g_BusExtension = busExt;

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

VOID HdBusUnload(PDRIVER_OBJECT DriverObject)
{
    PHD_BUS_EXTENSION busExt = g_BusExtension;

    if (busExt) {
        IoDeleteSymbolicLink(&busExt->BusSymLinkName);
        IoDeleteDevice(busExt->BusDeviceObject);
        g_BusExtension = NULL;
    }

    UNREFERENCED_PARAMETER(DriverObject);
}
