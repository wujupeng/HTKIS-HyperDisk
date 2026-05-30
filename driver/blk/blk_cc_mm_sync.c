#include "blk_driver.h"

NTSTATUS HdBlkAcquireForCcFlush(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);

    ExAcquireResourceSharedLite(&blkExt->FsRtlHeaderLock, TRUE);
    ExAcquireResourceExclusiveLite(&blkExt->CcFlushLock, TRUE);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkReleaseForCcFlush(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);

    ExReleaseResourceLite(&blkExt->CcFlushLock);
    ExReleaseResourceLite(&blkExt->FsRtlHeaderLock);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkAcquireForModWrite(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER EndingOffset,
    PERESOURCE Resource,
    PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(EndingOffset);
    UNREFERENCED_PARAMETER(Resource);

    ExAcquireResourceSharedLite(&blkExt->FsRtlHeaderLock, TRUE);
    ExAcquireResourceExclusiveLite(&blkExt->MmModWriteLock, TRUE);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkReleaseForModWrite(
    PFILE_OBJECT FileObject,
    PERESOURCE Resource,
    PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(Resource);

    ExReleaseResourceLite(&blkExt->MmModWriteLock);
    ExReleaseResourceLite(&blkExt->FsRtlHeaderLock);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkAcquireFileForNtCreateSection(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);

    ExAcquireResourceExclusiveLite(&blkExt->FsRtlHeaderLock, TRUE);

    return STATUS_SUCCESS;
}

NTSTATUS HdBlkReleaseFileForNtCreateSection(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(FileObject);

    ExReleaseResourceLite(&blkExt->FsRtlHeaderLock);

    return STATUS_SUCCESS;
}
