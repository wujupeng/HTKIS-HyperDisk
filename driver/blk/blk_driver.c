#include "blk_driver.h"

static HD_IRP_TRACKER g_IrpTracker;

static ULONGLONG HdQueryTickCount64(VOID)
{
    LARGE_INTEGER freq, count;
    KeQueryPerformanceCounter(&freq);
    count = KeQueryPerformanceCounter(NULL);
    return (ULONGLONG)(count.QuadPart / (freq.QuadPart / 1000));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverExtension->AddDevice = NULL;

    DriverObject->MajorFunction[IRP_MJ_CREATE]          = HdBlkDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = HdBlkDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_READ]            = HdBlkDispatchRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]           = HdBlkDispatchWrite;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]   = HdBlkDispatchFlushBuffers;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]        = HdBlkDispatchShutdown;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] = HdBlkDispatchQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]   = HdBlkDispatchSetInformation;
    DriverObject->MajorFunction[IRP_MJ_PNP]             = HdBlkDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = HdBlkDispatchIoctl;

    DriverObject->DriverUnload = NULL;

    HdIrpTrackerInit(&g_IrpTracker);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
    InterlockedIncrement(&blkExt->OpenHandleCount);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
    InterlockedDecrement(&blkExt->OpenHandleCount);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchFlushBuffers(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    KeSetEvent(&blkExt->IoEvent, IO_NO_INCREMENT, FALSE);
    KeWaitForSingleObject(&blkExt->IoEvent, Executive, KernelMode, FALSE, NULL);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchShutdown(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    KeSetEvent(&blkExt->ShutdownEvent, IO_NO_INCREMENT, FALSE);
    blkExt->IsStarted = FALSE;

    while (blkExt->PendingIoCount > 0) {
        LARGE_INTEGER delay;
        delay.QuadPart = -10 * 1000 * 10;
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchQueryInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;

    switch (irpSp->Parameters.QueryFile.FileInformationClass) {
    case FileStandardInformation: {
        PFILE_STANDARD_INFORMATION info = (PFILE_STANDARD_INFORMATION)buffer;
        info->AllocationSize.QuadPart = (LONGLONG)blkExt->DiskSize;
        info->EndOfFile.QuadPart = (LONGLONG)blkExt->DiskSize;
        info->NumberOfLinks = 1;
        info->DeletePending = FALSE;
        info->Directory = FALSE;
        Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);
        break;
    }
    case FileBasicInformation: {
        PFILE_BASIC_INFORMATION info = (PFILE_BASIC_INFORMATION)buffer;
        RtlZeroMemory(info, sizeof(FILE_BASIC_INFORMATION));
        info->FileAttributes = FILE_ATTRIBUTE_NORMAL;
        Irp->IoStatus.Information = sizeof(FILE_BASIC_INFORMATION);
        break;
    }
    default:
        Irp->IoStatus.Information = 0;
        break;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchSetInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->Parameters.SetFile.FileInformationClass) {
    case FileAllocationInformation: {
        PFILE_ALLOCATION_INFORMATION info = (PFILE_ALLOCATION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        blkExt->DiskSize = (ULONGLONG)info->AllocationSize.QuadPart;
        blkExt->BlockCount = (ULONG)(blkExt->DiskSize / blkExt->BlockSize);
        break;
    }
    case FileEndOfFileInformation: {
        PFILE_END_OF_FILE_INFORMATION info = (PFILE_END_OF_FILE_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        blkExt->DiskSize = (ULONGLONG)info->EndOfFile.QuadPart;
        blkExt->BlockCount = (ULONG)(blkExt->DiskSize / blkExt->BlockSize);
        break;
    }
    default:
        break;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBlkDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    switch (irpSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        blkExt->IsStarted = TRUE;
        break;
    case IRP_MN_REMOVE_DEVICE:
        blkExt->IsStarted = FALSE;
        break;
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        break;
    default:
        break;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}
