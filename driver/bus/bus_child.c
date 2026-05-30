#include "bus_driver.h"

NTSTATUS HdBusCreateChildDevice(PHD_BUS_EXTENSION BusExt, PHD_DEVICE_CONFIG Config)
{
    NTSTATUS status;
    PHD_CHILD_DEVICE child = NULL;
    KIRQL oldIrql;

    child = (PHD_CHILD_DEVICE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(HD_CHILD_DEVICE),
        HD_POOL_TAG
    );

    if (!child) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(child, sizeof(HD_CHILD_DEVICE));
    RtlCopyMemory(&child->Config, Config, sizeof(HD_DEVICE_CONFIG));

    child->ChildId = InterlockedIncrement((PLONG)&BusExt->ChildCount);

    KeAcquireSpinLock(&BusExt->ChildListLock, &oldIrql);
    InsertTailList(&BusExt->ChildListHead, &child->ListEntry);
    KeReleaseSpinLock(&BusExt->ChildListLock, oldIrql);

    return STATUS_SUCCESS;
}

NTSTATUS HdBusRemoveChildDevice(PHD_BUS_EXTENSION BusExt, ULONG ChildId)
{
    PLIST_ENTRY entry;
    PHD_CHILD_DEVICE child;
    KIRQL oldIrql;
    BOOLEAN found = FALSE;

    KeAcquireSpinLock(&BusExt->ChildListLock, &oldIrql);

    for (entry = BusExt->ChildListHead.Flink;
         entry != &BusExt->ChildListHead;
         entry = entry->Flink) {
        child = CONTAINING_RECORD(entry, HD_CHILD_DEVICE, ListEntry);
        if (child->ChildId == ChildId) {
            RemoveEntryList(&child->ListEntry);
            found = TRUE;
            break;
        }
    }

    KeReleaseSpinLock(&BusExt->ChildListLock, oldIrql);

    if (found) {
        ExFreePoolWithTag(child, HD_POOL_TAG);
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}
