#include "bus_driver.h"

NTSTATUS HdBusDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HdBusDispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesReturned = 0;
    PHD_BUS_EXTENSION busExt = (PHD_BUS_EXTENSION)DeviceObject->DeviceExtension;

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_HD_ADD_DEVICE: {
        PHD_DEVICE_CONFIG config = (PHD_DEVICE_CONFIG)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(HD_DEVICE_CONFIG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        status = HdBusCreateChildDevice(busExt, config);
        bytesReturned = 0;
        break;
    }

    case IOCTL_HD_REMOVE_DEVICE: {
        PULONG pChildId = (PULONG)Irp->AssociatedIrp.SystemBuffer;
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        status = HdBusRemoveChildDevice(busExt, *pChildId);
        bytesReturned = 0;
        break;
    }

    case IOCTL_HD_GET_STATUS:
        bytesReturned = 0;
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}
