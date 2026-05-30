#include "blk_driver.h"

BOOLEAN HdBlkFastIoRead(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    PVOID Buffer,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(LockKey);
    UNREFERENCED_PARAMETER(Wait);

    if (!blkExt->IsStarted) {
        return FALSE;
    }

    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = Length;
    return TRUE;
}

BOOLEAN HdBlkFastIoWrite(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    PVOID Buffer,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(LockKey);
    UNREFERENCED_PARAMETER(Wait);

    if (!blkExt->IsStarted) {
        return FALSE;
    }

    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = Length;
    return TRUE;
}

BOOLEAN HdBlkFastIoQueryBasicInfo(
    PFILE_OBJECT FileObject,
    BOOLEAN Wait,
    PFILE_BASIC_INFORMATION Buffer,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(DeviceObject);

    RtlZeroMemory(Buffer, sizeof(FILE_BASIC_INFORMATION));
    Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;
    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = sizeof(FILE_BASIC_INFORMATION);
    return TRUE;
}

BOOLEAN HdBlkFastIoQueryStandardInfo(
    PFILE_OBJECT FileObject,
    BOOLEAN Wait,
    PFILE_STANDARD_INFORMATION Buffer,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(Wait);

    Buffer->AllocationSize.QuadPart = (LONGLONG)blkExt->DiskSize;
    Buffer->EndOfFile.QuadPart = (LONGLONG)blkExt->DiskSize;
    Buffer->NumberOfLinks = 1;
    Buffer->DeletePending = FALSE;
    Buffer->Directory = FALSE;
    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = sizeof(FILE_STANDARD_INFORMATION);
    return TRUE;
}

BOOLEAN HdBlkFastIoDeviceControl(
    PFILE_OBJECT FileObject,
    BOOLEAN Wait,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    ULONG IoControlCode,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);
    UNREFERENCED_PARAMETER(DeviceObject);

    IoStatus->Status = STATUS_NOT_SUPPORTED;
    return FALSE;
}
