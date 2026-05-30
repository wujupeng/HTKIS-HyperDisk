#include "blk_driver.h"

NTSTATUS HdBlkDispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesReturned = 0;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_HD_SET_CONFIG: {
        PHD_DEVICE_CONFIG config = (PHD_DEVICE_CONFIG)Irp->AssociatedIrp.SystemBuffer;
        PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(HD_DEVICE_CONFIG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        blkExt->ImageId    = config->ImageId;
        blkExt->DiskSize   = config->DiskSize;
        blkExt->BlockSize  = config->BlockSize;
        blkExt->BlockCount = config->BlockCount;
        blkExt->LayerId    = config->LayerId;
        blkExt->IsStarted  = TRUE;
        bytesReturned = 0;
        break;
    }

    case IOCTL_HD_GET_STATUS: {
        PHD_BLK_EXTENSION blkExt = (PHD_BLK_EXTENSION)DeviceObject->DeviceExtension;
        PHD_BLOCK_RESPONSE resp = (PHD_BLOCK_RESPONSE)Irp->AssociatedIrp.SystemBuffer;
        resp->Status = blkExt->IsStarted ? 0 : -1;
        bytesReturned = sizeof(HD_BLOCK_RESPONSE);
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
