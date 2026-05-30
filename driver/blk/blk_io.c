#include "blk_driver.h"

NTSTATUS HdBlkDispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    HD_BLOCK_REQUEST request = {0};
    request.ImageId      = blkExt->ImageId;
    request.BlockOffset  = irpSp->Parameters.Read.ByteOffset.QuadPart / blkExt->BlockSize;
    request.BlockCount   = irpSp->Parameters.Read.Length / blkExt->BlockSize;
    request.LayerId      = blkExt->LayerId;
    request.IsWrite      = 0;

    status = HdBlkSubmitBlockRequest(blkExt, &request);

    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    return STATUS_PENDING;
}

NTSTATUS HdBlkDispatchWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    HD_BLOCK_REQUEST request = {0};
    request.ImageId      = blkExt->ImageId;
    request.BlockOffset  = irpSp->Parameters.Write.ByteOffset.QuadPart / blkExt->BlockSize;
    request.BlockCount   = irpSp->Parameters.Write.Length / blkExt->BlockSize;
    request.LayerId      = blkExt->LayerId;
    request.IsWrite      = 1;

    status = HdBlkSubmitBlockRequest(blkExt, &request);

    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    return STATUS_PENDING;
}
