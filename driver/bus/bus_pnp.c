#include "bus_driver.h"

NTSTATUS HdBusDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status = STATUS_SUCCESS;
    PHD_BUS_EXTENSION busExt = (PHD_BUS_EXTENSION)DeviceObject->DeviceExtension;

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->MinorFunction) {
    case IRP_MN_START_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(DeviceObject, Irp);
        return status;

    case IRP_MN_REMOVE_DEVICE:
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(DeviceObject, Irp);
        return status;

    case IRP_MN_QUERY_DEVICE_RELATIONS:
        if (irpSp->Parameters.QueryDeviceRelations.Type == BusRelations) {
            KIRQL oldIrql;
            ULONG childCount = 0;
            ULONG i = 0;
            PDEVICE_RELATIONS relations = NULL;
            PLIST_ENTRY entry;

            KeAcquireSpinLock(&busExt->ChildListLock, &oldIrql);
            for (entry = busExt->ChildListHead.Flink;
                 entry != &busExt->ChildListHead;
                 entry = entry->Flink) {
                childCount++;
            }

            ULONG relationSize = sizeof(DEVICE_RELATIONS) + (childCount * sizeof(PDEVICE_OBJECT));
            relations = (PDEVICE_RELATIONS)ExAllocatePool2(
                POOL_FLAG_NON_PAGED | POOL_FLAG_PAGED,
                relationSize,
                HD_POOL_TAG
            );

            if (relations) {
                relations->Count = 0;
                for (entry = busExt->ChildListHead.Flink;
                     entry != &busExt->ChildListHead;
                     entry = entry->Flink) {
                    PHD_CHILD_DEVICE child = CONTAINING_RECORD(entry, HD_CHILD_DEVICE, ListEntry);
                    if (child->ChildDeviceObject) {
                        ObReferenceObject(child->ChildDeviceObject);
                        relations->Objects[relations->Count++] = child->ChildDeviceObject;
                    }
                }
            }
            KeReleaseSpinLock(&busExt->ChildListLock, oldIrql);

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
    status = IoCallDriver(DeviceObject, Irp);
    return status;
}
