#define INITGUID
#include "wdm_disk_driver.h"

static PHD_BLK_DEVICE g_BlkDevice = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symLinkName;
    PDEVICE_OBJECT deviceObject = NULL;
    PHD_BLK_DEVICE blkDev = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&deviceName, HD_BLK_DEVICE_NAME);
    RtlInitUnicodeString(&symLinkName, HD_BLK_SYM_LINK);

    status = IoCreateDevice(
        DriverObject,
        sizeof(HD_BLK_DEVICE),
        &deviceName,
        FILE_DEVICE_DISK,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    blkDev = (PHD_BLK_DEVICE)deviceObject->DeviceExtension;
    RtlZeroMemory(blkDev, sizeof(HD_BLK_DEVICE));

    blkDev->DeviceObject      = deviceObject;
    blkDev->DiskSize          = HD_DISK_SIZE;
    blkDev->SectorSize        = HD_SECTOR_SIZE;
    blkDev->TotalSectors      = HD_TOTAL_SECTORS;
    blkDev->SectorsPerTrack   = 63;
    blkDev->TracksPerCylinder = 255;
    blkDev->Cylinders         = HD_CYLINDERS;
    if (blkDev->Cylinders == 0) blkDev->Cylinders = 1;
    blkDev->Initialized       = TRUE;

    deviceObject->Flags |= DO_DIRECT_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    status = IoCreateSymbolicLink(&symLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    status = IoRegisterDeviceInterface(
        deviceObject,
        &MOUNTDEV_MOUNTED_DEVICE_GUID,
        NULL,
        &blkDev->InterfaceName
    );

    if (NT_SUCCESS(status)) {
        blkDev->InterfaceEnabled = TRUE;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = HdBlkDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = HdBlkDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_READ]           = HdBlkDispatchRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]          = HdBlkDispatchWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HdBlkDispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP]            = HdBlkDispatchPnp;
    DriverObject->DriverUnload                          = HdBlkUnload;

    g_BlkDevice = blkDev;

    if (blkDev->InterfaceEnabled) {
        IoSetDeviceInterfaceState(&blkDev->InterfaceName, TRUE);
    }

    HdBlkNotifyMountManager(deviceObject);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkNotifyMountManager(PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS status;
    UNICODE_STRING mountMgrName;
    PFILE_OBJECT mountMgrFileObj = NULL;
    PDEVICE_OBJECT mountMgrDevice = NULL;
    PIRP irp = NULL;
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    PMOUNTMGR_TARGET_NAME targetName = NULL;
    ULONG targetNameSize;
    UNICODE_STRING targetNameStr;
    PIO_STACK_LOCATION irpSp;
    ULONG ioctlCode;

    RtlInitUnicodeString(&mountMgrName, MOUNTMGR_DEVICE_NAME);

    status = IoGetDeviceObjectPointer(&mountMgrName, FILE_READ_ATTRIBUTES, &mountMgrFileObj, &mountMgrDevice);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    RtlInitUnicodeString(&targetNameStr, HD_BLK_DEVICE_NAME);

    targetNameSize = sizeof(MOUNTMGR_TARGET_NAME) + targetNameStr.Length - sizeof(WCHAR);
    targetName = (PMOUNTMGR_TARGET_NAME)ExAllocatePoolWithTag(NonPagedPool, targetNameSize, HD_BLK_POOL_TAG);
    if (!targetName) {
        ObDereferenceObject(mountMgrFileObj);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    targetName->DeviceNameLength = targetNameStr.Length;
    RtlCopyMemory(targetName->DeviceName, targetNameStr.Buffer, targetNameStr.Length);

    ioctlCode = CTL_CODE(MOUNTDEVCONTROLTYPE, 5, METHOD_BUFFERED, FILE_READ_ACCESS);
    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
        ioctlCode,
        mountMgrDevice,
        targetName,
        targetNameSize,
        NULL,
        0,
        FALSE,
        &event,
        &ioStatus
    );

    if (!irp) {
        ExFreePoolWithTag(targetName, HD_BLK_POOL_TAG);
        ObDereferenceObject(mountMgrFileObj);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver(mountMgrDevice, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    ExFreePoolWithTag(targetName, HD_BLK_POOL_TAG);
    ObDereferenceObject(mountMgrFileObj);

    UNREFERENCED_PARAMETER(DeviceObject);
    return status;
}

VOID HdBlkUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symLinkName;

    if (g_BlkDevice) {
        if (g_BlkDevice->InterfaceEnabled) {
            IoSetDeviceInterfaceState(&g_BlkDevice->InterfaceName, FALSE);
            RtlFreeUnicodeString(&g_BlkDevice->InterfaceName);
        }
        RtlInitUnicodeString(&symLinkName, HD_BLK_SYM_LINK);
        IoDeleteSymbolicLink(&symLinkName);
        IoDeleteDevice(g_BlkDevice->DeviceObject);
        g_BlkDevice = NULL;
    }

    UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS HdBlkDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_DEVICE blkDev = (PHD_BLK_DEVICE)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONGLONG byteOffset = irpSp->Parameters.Read.ByteOffset.QuadPart;
    ULONG readLength = irpSp->Parameters.Read.Length;

    if (byteOffset + readLength > (LONGLONG)blkDev->DiskSize) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
    } else {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = readLength;
    }
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS HdBlkDispatchWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_DEVICE blkDev = (PHD_BLK_DEVICE)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONGLONG byteOffset = irpSp->Parameters.Write.ByteOffset.QuadPart;
    ULONG writeLength = irpSp->Parameters.Write.Length;

    if (byteOffset + writeLength > (LONGLONG)blkDev->DiskSize) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
    } else {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = writeLength;
    }
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS HdBlkDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    case IRP_MN_REMOVE_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_QUERY_REMOVE_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_CANCEL_REMOVE_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    case IRP_MN_SURPRISE_REMOVAL:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    default:
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        break;
    }

    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    UNREFERENCED_PARAMETER(DeviceObject);
    return Irp->IoStatus.Status;
}

NTSTATUS HdBlkDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_DEVICE blkDev = (PHD_BLK_DEVICE)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesReturned = 0;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_DISK_GET_DRIVE_GEOMETRY: {
        PDISK_GEOMETRY geometry = (PDISK_GEOMETRY)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DISK_GEOMETRY)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        RtlZeroMemory(geometry, sizeof(DISK_GEOMETRY));
        geometry->Cylinders.QuadPart         = (LONGLONG)blkDev->Cylinders;
        geometry->MediaType                   = FixedMedia;
        geometry->TracksPerCylinder           = blkDev->TracksPerCylinder;
        geometry->SectorsPerTrack             = blkDev->SectorsPerTrack;
        geometry->BytesPerSector              = blkDev->SectorSize;
        bytesReturned = sizeof(DISK_GEOMETRY);
        break;
    }

    case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX: {
        PDISK_GEOMETRY_EX geometryEx = (PDISK_GEOMETRY_EX)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DISK_GEOMETRY_EX)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        RtlZeroMemory(geometryEx, sizeof(DISK_GEOMETRY_EX));
        geometryEx->Geometry.Cylinders.QuadPart = (LONGLONG)blkDev->Cylinders;
        geometryEx->Geometry.MediaType          = FixedMedia;
        geometryEx->Geometry.TracksPerCylinder  = blkDev->TracksPerCylinder;
        geometryEx->Geometry.SectorsPerTrack    = blkDev->SectorsPerTrack;
        geometryEx->Geometry.BytesPerSector     = blkDev->SectorSize;
        geometryEx->DiskSize.QuadPart           = (LONGLONG)blkDev->DiskSize;
        bytesReturned = sizeof(DISK_GEOMETRY_EX);
        break;
    }

    case IOCTL_DISK_GET_LENGTH_INFO: {
        PGET_LENGTH_INFORMATION lengthInfo = (PGET_LENGTH_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_LENGTH_INFORMATION)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        lengthInfo->Length.QuadPart = (LONGLONG)blkDev->DiskSize;
        bytesReturned = sizeof(GET_LENGTH_INFORMATION);
        break;
    }

    case IOCTL_DISK_IS_WRITABLE:
        status = STATUS_SUCCESS;
        bytesReturned = 0;
        break;

    case IOCTL_DISK_MEDIA_REMOVAL:
        status = STATUS_SUCCESS;
        bytesReturned = 0;
        break;

    case IOCTL_STORAGE_GET_MEDIA_TYPES_EX: {
        PGET_MEDIA_TYPES mediaTypes = (PGET_MEDIA_TYPES)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_MEDIA_TYPES)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        RtlZeroMemory(mediaTypes, sizeof(GET_MEDIA_TYPES));
        mediaTypes->DeviceType = FILE_DEVICE_DISK;
        mediaTypes->MediaInfoCount = 1;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.MediaType = (STORAGE_MEDIA_TYPE)FixedMedia;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.NumberMediaSides = 1;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.MediaCharacteristics = MEDIA_CURRENTLY_MOUNTED | MEDIA_READ_WRITE;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.Cylinders.QuadPart = (LONGLONG)blkDev->Cylinders;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.TracksPerCylinder = blkDev->TracksPerCylinder;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.SectorsPerTrack = blkDev->SectorsPerTrack;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.BytesPerSector = blkDev->SectorSize;
        bytesReturned = sizeof(GET_MEDIA_TYPES);
        break;
    }

    case IOCTL_STORAGE_GET_DEVICE_NUMBER: {
        PSTORAGE_DEVICE_NUMBER deviceNumber = (PSTORAGE_DEVICE_NUMBER)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(STORAGE_DEVICE_NUMBER)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        deviceNumber->DeviceType = FILE_DEVICE_DISK;
        deviceNumber->DeviceNumber = 1;
        deviceNumber->PartitionNumber = (ULONG)-1;
        bytesReturned = sizeof(STORAGE_DEVICE_NUMBER);
        break;
    }

    case IOCTL_STORAGE_QUERY_PROPERTY: {
        PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(STORAGE_PROPERTY_QUERY)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        if (query->PropertyId == StorageDeviceProperty) {
            PSTORAGE_DEVICE_DESCRIPTOR devDesc = (PSTORAGE_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;
            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            RtlZeroMemory(devDesc, sizeof(STORAGE_DEVICE_DESCRIPTOR));
            devDesc->Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
            devDesc->Size = sizeof(STORAGE_DEVICE_DESCRIPTOR);
            devDesc->DeviceType = FILE_DEVICE_DISK;
            devDesc->BusType = BusTypeVirtual;
            bytesReturned = sizeof(STORAGE_DEVICE_DESCRIPTOR);
        } else if (query->PropertyId == StorageAdapterProperty) {
            PSTORAGE_ADAPTER_DESCRIPTOR adapDesc = (PSTORAGE_ADAPTER_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;
            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(STORAGE_ADAPTER_DESCRIPTOR)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            RtlZeroMemory(adapDesc, sizeof(STORAGE_ADAPTER_DESCRIPTOR));
            adapDesc->Version = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
            adapDesc->Size = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
            adapDesc->MaximumTransferLength = 128 * 1024;
            adapDesc->MaximumPhysicalPages = 32;
            adapDesc->AlignmentMask = 0;
            adapDesc->BusType = BusTypeVirtual;
            bytesReturned = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;
    }

    case IOCTL_DISK_GET_PARTITION_INFO_EX: {
        PPARTITION_INFORMATION_EX partInfo = (PPARTITION_INFORMATION_EX)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(PARTITION_INFORMATION_EX)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        RtlZeroMemory(partInfo, sizeof(PARTITION_INFORMATION_EX));
        partInfo->PartitionStyle = PARTITION_STYLE_MBR;
        partInfo->StartingOffset.QuadPart = 0;
        partInfo->PartitionLength.QuadPart = (LONGLONG)blkDev->DiskSize;
        partInfo->Mbr.PartitionType = PARTITION_IFS;
        partInfo->Mbr.BootIndicator = FALSE;
        partInfo->Mbr.RecognizedPartition = TRUE;
        partInfo->Mbr.HiddenSectors = 0;
        partInfo->RewritePartition = FALSE;
        bytesReturned = sizeof(PARTITION_INFORMATION_EX);
        break;
    }

    case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME: {
        PMOUNTDEV_NAME mountName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
        UNICODE_STRING nameStr;
        RtlInitUnicodeString(&nameStr, HD_BLK_SYM_LINK);
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MOUNTDEV_NAME) + nameStr.Length) {
            status = STATUS_BUFFER_OVERFLOW;
            Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME) + nameStr.Length;
            break;
        }
        mountName->NameLength = nameStr.Length;
        RtlCopyMemory((PUCHAR)mountName + sizeof(MOUNTDEV_NAME), nameStr.Buffer, nameStr.Length);
        bytesReturned = sizeof(MOUNTDEV_NAME) + nameStr.Length;
        break;
    }

    case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID: {
        PMOUNTDEV_UNIQUE_ID uniqueId = (PMOUNTDEV_UNIQUE_ID)Irp->AssociatedIrp.SystemBuffer;
        UCHAR idStr[] = "HyperDisk_VDisk0";
        ULONG idLen = sizeof(idStr) - 1;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MOUNTDEV_UNIQUE_ID) + idLen) {
            status = STATUS_BUFFER_OVERFLOW;
            Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID) + idLen;
            break;
        }
        uniqueId->UniqueIdLength = idLen;
        RtlCopyMemory((PUCHAR)uniqueId + sizeof(MOUNTDEV_UNIQUE_ID), idStr, idLen);
        bytesReturned = sizeof(MOUNTDEV_UNIQUE_ID) + idLen;
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
