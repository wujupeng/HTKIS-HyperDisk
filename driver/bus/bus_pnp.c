#define INITGUID
#include "bus_driver.h"

extern PHD_BUS_EXTENSION g_BusExtension;

static BOOLEAN HdBusIsFdo(PDEVICE_OBJECT DeviceObject)
{
    return (DeviceObject == g_BusExtension->BusDeviceObject);
}

static PHD_CHILD_DEVICE HdBusFindChildByPdo(PDEVICE_OBJECT Pdo)
{
    PLIST_ENTRY entry;
    KIRQL oldIrql;
    PHD_CHILD_DEVICE found = NULL;

    KeAcquireSpinLock(&g_BusExtension->ChildListLock, &oldIrql);
    for (entry = g_BusExtension->ChildListHead.Flink;
         entry != &g_BusExtension->ChildListHead;
         entry = entry->Flink) {
        PHD_CHILD_DEVICE child = CONTAINING_RECORD(entry, HD_CHILD_DEVICE, ListEntry);
        if (child->ChildDeviceObject == Pdo) {
            found = child;
            break;
        }
    }
    KeReleaseSpinLock(&g_BusExtension->ChildListLock, oldIrql);
    return found;
}

NTSTATUS HdBusDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBusDispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PoStartNextPowerIrp(Irp);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBusDispatchSystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS HdBusCreatePdo(PHD_BUS_EXTENSION BusExt, PHD_CHILD_DEVICE Child)
{
    NTSTATUS status;
    UNICODE_STRING pdoName;
    WCHAR pdoNameBuf[64];

    swprintf(pdoNameBuf, 64, L"\\Device\\HyperDiskDisk%d", Child->ChildId);
    RtlInitUnicodeString(&pdoName, pdoNameBuf);

    status = IoCreateDevice(
        BusExt->BusDeviceObject->DriverObject,
        sizeof(HD_CHILD_DEVICE),
        &pdoName,
        FILE_DEVICE_DISK,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &Child->ChildDeviceObject
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    Child->ChildDeviceObject->Flags |= DO_DIRECT_IO;
    Child->ChildDeviceObject->Flags |= DO_POWER_PAGABLE;
    Child->ChildDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    PHD_CHILD_DEVICE pdoExt = (PHD_CHILD_DEVICE)Child->ChildDeviceObject->DeviceExtension;
    RtlCopyMemory(pdoExt, Child, sizeof(HD_CHILD_DEVICE));
    pdoExt->ChildDeviceObject = Child->ChildDeviceObject;

    return STATUS_SUCCESS;
}

static NTSTATUS HdBusAutoEnumerateChild(PHD_BUS_EXTENSION BusExt)
{
    NTSTATUS status;
    PHD_CHILD_DEVICE child = NULL;
    KIRQL oldIrql;

    child = (PHD_CHILD_DEVICE)ExAllocatePoolWithTag(NonPagedPool, sizeof(HD_CHILD_DEVICE), HD_POOL_TAG);
    if (!child) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(child, sizeof(HD_CHILD_DEVICE));
    child->ChildId = InterlockedIncrement((PLONG)&BusExt->ChildCount);

    child->DiskSize          = HD_DISK_SIZE;
    child->SectorSize        = HD_SECTOR_SIZE;
    child->TotalSectors      = HD_TOTAL_SECTORS;
    child->SectorsPerTrack   = 63;
    child->TracksPerCylinder = 255;
    child->Cylinders         = HD_CYLINDERS;
    if (child->Cylinders == 0) child->Cylinders = 1;
    child->Started           = FALSE;

    status = HdBusCreatePdo(BusExt, child);
    if (!NT_SUCCESS(status)) {
        ExFreePoolWithTag(child, HD_POOL_TAG);
        return status;
    }

    KeAcquireSpinLock(&BusExt->ChildListLock, &oldIrql);
    InsertTailList(&BusExt->ChildListHead, &child->ListEntry);
    KeReleaseSpinLock(&BusExt->ChildListLock, oldIrql);

    return STATUS_SUCCESS;
}

static NTSTATUS HdBusPdoQueryId(PHD_CHILD_DEVICE Child, PIO_STACK_LOCATION IrpSp, PIRP Irp)
{
    PWCHAR idBuffer = NULL;
    ULONG idLength;
    UNICODE_STRING idStr;

    switch (IrpSp->Parameters.QueryId.IdType) {
    case BusQueryDeviceID:
        RtlInitUnicodeString(&idStr, L"ROOT\\HYPERDISKBLK");
        idLength = idStr.Length + sizeof(WCHAR) + sizeof(WCHAR);
        idBuffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool, idLength, HD_POOL_TAG);
        if (!idBuffer) return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(idBuffer, idLength);
        RtlCopyMemory(idBuffer, idStr.Buffer, idStr.Length);
        Irp->IoStatus.Information = (ULONG_PTR)idBuffer;
        return STATUS_SUCCESS;

    case BusQueryHardwareIDs: {
        PWCHAR hwIds[] = { L"ROOT\\HYPERDISKBLK", L"HTKIS\\HyperDisk_VDisk", NULL };
        ULONG totalLen = 0;
        ULONG i;
        for (i = 0; hwIds[i] != NULL; i++) {
            totalLen += (wcslen(hwIds[i]) + 1) * sizeof(WCHAR);
        }
        totalLen += sizeof(WCHAR);
        idBuffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool, totalLen, HD_POOL_TAG);
        if (!idBuffer) return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(idBuffer, totalLen);
        PWCHAR ptr = idBuffer;
        for (i = 0; hwIds[i] != NULL; i++) {
            RtlCopyMemory(ptr, hwIds[i], wcslen(hwIds[i]) * sizeof(WCHAR));
            ptr += wcslen(hwIds[i]) + 1;
        }
        Irp->IoStatus.Information = (ULONG_PTR)idBuffer;
        return STATUS_SUCCESS;
    }

    case BusQueryInstanceID:
        RtlInitUnicodeString(&idStr, L"0");
        idLength = idStr.Length + sizeof(WCHAR) + sizeof(WCHAR);
        idBuffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool, idLength, HD_POOL_TAG);
        if (!idBuffer) return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(idBuffer, idLength);
        RtlCopyMemory(idBuffer, idStr.Buffer, idStr.Length);
        Irp->IoStatus.Information = (ULONG_PTR)idBuffer;
        return STATUS_SUCCESS;

    case BusQueryCompatibleIDs: {
        PWCHAR compatIds[] = { L"ROOT\\DISK", NULL };
        ULONG totalLen = 0;
        ULONG i;
        for (i = 0; compatIds[i] != NULL; i++) {
            totalLen += (wcslen(compatIds[i]) + 1) * sizeof(WCHAR);
        }
        totalLen += sizeof(WCHAR);
        idBuffer = (PWCHAR)ExAllocatePoolWithTag(PagedPool, totalLen, HD_POOL_TAG);
        if (!idBuffer) return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(idBuffer, totalLen);
        PWCHAR ptr = idBuffer;
        for (i = 0; compatIds[i] != NULL; i++) {
            RtlCopyMemory(ptr, compatIds[i], wcslen(compatIds[i]) * sizeof(WCHAR));
            ptr += wcslen(compatIds[i]) + 1;
        }
        Irp->IoStatus.Information = (ULONG_PTR)idBuffer;
        return STATUS_SUCCESS;
    }

    default:
        return STATUS_NOT_SUPPORTED;
    }
}

static NTSTATUS HdBusPdoPnp(PHD_CHILD_DEVICE Child, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;

    switch (irpSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        Child->Started = TRUE;
        if (!Child->InterfaceRegistered) {
            status = IoRegisterDeviceInterface(
                Child->ChildDeviceObject,
                &MOUNTDEV_MOUNTED_DEVICE_GUID,
                NULL,
                &Child->InterfaceName
            );
            if (NT_SUCCESS(status)) {
                Child->InterfaceRegistered = TRUE;
            }
        }
        if (Child->InterfaceRegistered) {
            IoSetDeviceInterfaceState(&Child->InterfaceName, TRUE);
        }
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_CANCEL_REMOVE_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_REMOVE_DEVICE:
        if (Child->InterfaceRegistered) {
            IoSetDeviceInterfaceState(&Child->InterfaceName, FALSE);
            RtlFreeUnicodeString(&Child->InterfaceName);
            Child->InterfaceRegistered = FALSE;
        }
        Child->Started = FALSE;
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_SURPRISE_REMOVAL:
        if (Child->InterfaceRegistered) {
            IoSetDeviceInterfaceState(&Child->InterfaceName, FALSE);
        }
        Child->Started = FALSE;
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_ID:
        status = HdBusPdoQueryId(Child, irpSp, Irp);
        Irp->IoStatus.Status = status;
        break;

    case IRP_MN_QUERY_CAPABILITIES: {
        PDEVICE_CAPABILITIES caps = irpSp->Parameters.DeviceCapabilities.Capabilities;
        if (caps) {
            caps->Removable = FALSE;
            caps->SurpriseRemovalOK = TRUE;
            caps->EjectSupported = FALSE;
            caps->LockSupported = FALSE;
            caps->D1Latency = 0;
            caps->D2Latency = 0;
            caps->D3Latency = 0;
        }
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    }

    case IRP_MN_QUERY_DEVICE_TEXT: {
        if (irpSp->Parameters.QueryDeviceText.DeviceTextType == DeviceTextDescription) {
            WCHAR desc[] = L"HTKIS HyperDisk Virtual Disk";
            ULONG descLen = sizeof(desc);
            PWCHAR textBuf = (PWCHAR)ExAllocatePoolWithTag(PagedPool, descLen, HD_POOL_TAG);
            if (textBuf) {
                RtlCopyMemory(textBuf, desc, descLen);
                Irp->IoStatus.Information = (ULONG_PTR)textBuf;
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
        }
        Irp->IoStatus.Status = status;
        break;
    }

    default:
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (status != STATUS_PENDING) {
        Irp->IoStatus.Status = (Irp->IoStatus.Status != STATUS_SUCCESS) ? Irp->IoStatus.Status : status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    return Irp->IoStatus.Status;
}

static NTSTATUS HdBusFdoPnp(PHD_BUS_EXTENSION BusExt, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        HdBusAutoEnumerateChild(BusExt);
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        if (irpSp->Parameters.QueryDeviceRelations.Type == BusRelations) {
            KIRQL oldIrql;
            ULONG childCount = 0;
            ULONG i = 0;
            PDEVICE_RELATIONS relations = NULL;
            PLIST_ENTRY entry;

            KeAcquireSpinLock(&BusExt->ChildListLock, &oldIrql);
            for (entry = BusExt->ChildListHead.Flink;
                 entry != &BusExt->ChildListHead;
                 entry = entry->Flink) {
                childCount++;
            }

            ULONG relationSize = sizeof(DEVICE_RELATIONS) + (childCount * sizeof(PDEVICE_OBJECT));
            relations = (PDEVICE_RELATIONS)ExAllocatePoolWithTag(NonPagedPool, relationSize, HD_POOL_TAG);

            if (relations) {
                relations->Count = 0;
                for (entry = BusExt->ChildListHead.Flink;
                     entry != &BusExt->ChildListHead;
                     entry = entry->Flink) {
                    PHD_CHILD_DEVICE child = CONTAINING_RECORD(entry, HD_CHILD_DEVICE, ListEntry);
                    if (child->ChildDeviceObject) {
                        ObReferenceObject(child->ChildDeviceObject);
                        relations->Objects[relations->Count++] = child->ChildDeviceObject;
                    }
                }
            }
            KeReleaseSpinLock(&BusExt->ChildListLock, oldIrql);

            if (relations) {
                if (Irp->IoStatus.Information != 0) {
                    PDEVICE_RELATIONS oldRelations = (PDEVICE_RELATIONS)Irp->IoStatus.Information;
                    for (i = 0; i < oldRelations->Count; i++) {
                        ObDereferenceObject(oldRelations->Objects[i]);
                    }
                    ExFreePoolWithTag(oldRelations, 0);
                }
                Irp->IoStatus.Information = (ULONG_PTR)relations;
                Irp->IoStatus.Status = STATUS_SUCCESS;
            } else {
                Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            }
        }
        break;

    default:
        break;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(BusExt->BusDeviceObject, Irp);
}

NTSTATUS HdBusDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    if (HdBusIsFdo(DeviceObject)) {
        return HdBusFdoPnp(g_BusExtension, Irp);
    } else {
        PHD_CHILD_DEVICE child = HdBusFindChildByPdo(DeviceObject);
        if (child) {
            return HdBusPdoPnp(child, Irp);
        }
        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }
}

NTSTATUS HdBusDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    if (HdBusIsFdo(DeviceObject)) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    PHD_CHILD_DEVICE child = HdBusFindChildByPdo(DeviceObject);
    if (!child) {
        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }

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
        geometry->Cylinders.QuadPart         = (LONGLONG)child->Cylinders;
        geometry->MediaType                   = FixedMedia;
        geometry->TracksPerCylinder           = child->TracksPerCylinder;
        geometry->SectorsPerTrack             = child->SectorsPerTrack;
        geometry->BytesPerSector              = child->SectorSize;
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
        geometryEx->Geometry.Cylinders.QuadPart = (LONGLONG)child->Cylinders;
        geometryEx->Geometry.MediaType          = FixedMedia;
        geometryEx->Geometry.TracksPerCylinder  = child->TracksPerCylinder;
        geometryEx->Geometry.SectorsPerTrack    = child->SectorsPerTrack;
        geometryEx->Geometry.BytesPerSector     = child->SectorSize;
        geometryEx->DiskSize.QuadPart           = (LONGLONG)child->DiskSize;
        bytesReturned = sizeof(DISK_GEOMETRY_EX);
        break;
    }

    case IOCTL_DISK_GET_LENGTH_INFO: {
        PGET_LENGTH_INFORMATION lengthInfo = (PGET_LENGTH_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(GET_LENGTH_INFORMATION)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        lengthInfo->Length.QuadPart = (LONGLONG)child->DiskSize;
        bytesReturned = sizeof(GET_LENGTH_INFORMATION);
        break;
    }

    case IOCTL_DISK_IS_WRITABLE:
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
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.Cylinders.QuadPart = (LONGLONG)child->Cylinders;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.TracksPerCylinder = child->TracksPerCylinder;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.SectorsPerTrack = child->SectorsPerTrack;
        mediaTypes->MediaInfo[0].DeviceSpecific.DiskInfo.BytesPerSector = child->SectorSize;
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
        deviceNumber->DeviceNumber = 0;
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
        partInfo->PartitionLength.QuadPart = (LONGLONG)child->DiskSize;
        partInfo->Mbr.PartitionType = PARTITION_IFS;
        partInfo->Mbr.BootIndicator = FALSE;
        partInfo->Mbr.RecognizedPartition = TRUE;
        partInfo->Mbr.HiddenSectors = 0;
        partInfo->RewritePartition = FALSE;
        bytesReturned = sizeof(PARTITION_INFORMATION_EX);
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
