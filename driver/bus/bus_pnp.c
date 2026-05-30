#include "bus_driver.h"

NTSTATUS HdBusDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status = STATUS_SUCCESS;

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        status = IoCallDriver(DeviceObject, Irp);
        break;

    case IRP_MN_REMOVE_DEVICE:
        status = IoCallDriver(DeviceObject, Irp);
        break;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        status = IoCallDriver(DeviceObject, Irp);
        break;

    default:
        status = IoCallDriver(DeviceObject, Irp);
        break;
    }

    return status;
}
