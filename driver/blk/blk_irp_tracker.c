#include "blk_driver.h"

extern HD_IRP_TRACKER g_IrpTracker;

static ULONGLONG HdQueryTickCountMs(VOID)
{
    LARGE_INTEGER freq, count;
    KeQueryPerformanceCounter(&freq);
    count = KeQueryPerformanceCounter(NULL);
    return (ULONGLONG)(count.QuadPart * 1000 / freq.QuadPart);
}

VOID HdIrpTrackerInit(PHD_IRP_TRACKER Tracker)
{
    KeInitializeSpinLock(&Tracker->Lock);
    InitializeListHead(&Tracker->ActiveListHead);
    Tracker->ActiveCount = 0;
    Tracker->TimerRunning = FALSE;

    KeInitializeDpc(&Tracker->WatchdogDpc, HdIrpTrackerWatchdog, Tracker);
    KeInitializeTimer(&Tracker->WatchdogTimer);
}

VOID HdIrpTrackerDestroy(PHD_IRP_TRACKER Tracker)
{
    if (Tracker->TimerRunning) {
        KeCancelTimer(&Tracker->WatchdogTimer);
        Tracker->TimerRunning = FALSE;
    }
    KeRemoveQueueDpc(&Tracker->WatchdogDpc);
}

VOID HdIrpTrackerSubmit(PHD_IRP_TRACKER Tracker, PIRP Irp, UCHAR MajorFn, UCHAR MinorFn)
{
    KIRQL oldIrql;
    PHD_IRP_TRACK_ENTRY entry;

    entry = (PHD_IRP_TRACK_ENTRY)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(HD_IRP_TRACK_ENTRY), 'TRPH');
    if (!entry) return;

    entry->Irp = Irp;
    entry->SubmitTimestamp = HdQueryTickCountMs();
    entry->MajorFunction = MajorFn;
    entry->MinorFunction = MinorFn;
    entry->IoSize = 0;

    ObReferenceObject(Irp);

    KeAcquireSpinLock(&Tracker->Lock, &oldIrql);
    InsertTailList(&Tracker->ActiveListHead, &entry->ListEntry);
    Tracker->ActiveCount++;
    KeReleaseSpinLock(&Tracker->Lock, oldIrql);

    if (!Tracker->TimerRunning) {
        LARGE_INTEGER period;
        period.QuadPart = -10 * 1000 * 1000 * HD_IRP_SCAN_INTERVAL_SEC;
        KeSetTimerEx(&Tracker->WatchdogTimer, period, HD_IRP_SCAN_INTERVAL_SEC * 1000, &Tracker->WatchdogDpc);
        Tracker->TimerRunning = TRUE;
    }
}

VOID HdIrpTrackerComplete(PHD_IRP_TRACKER Tracker, PIRP Irp)
{
    KIRQL oldIrql;
    PLIST_ENTRY entry;
    PHD_IRP_TRACK_ENTRY found = NULL;

    KeAcquireSpinLock(&Tracker->Lock, &oldIrql);

    for (entry = Tracker->ActiveListHead.Flink;
         entry != &Tracker->ActiveListHead;
         entry = entry->Flink) {
        PHD_IRP_TRACK_ENTRY e = CONTAINING_RECORD(entry, HD_IRP_TRACK_ENTRY, ListEntry);
        if (e->Irp == Irp) {
            RemoveEntryList(&e->ListEntry);
            found = e;
            Tracker->ActiveCount--;
            break;
        }
    }

    KeReleaseSpinLock(&Tracker->Lock, oldIrql);

    if (found) {
        ObDereferenceObject(Irp);
        ExFreePoolWithTag(found, 'TRPH');
    }
}

VOID HdIrpTrackerWatchdog(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    PHD_IRP_TRACKER tracker = (PHD_IRP_TRACKER)DeferredContext;
    ULONGLONG now = HdQueryTickCountMs();
    KIRQL oldIrql;
    PLIST_ENTRY entry, next;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    KeAcquireSpinLock(&tracker->Lock, &oldIrql);

    for (entry = tracker->ActiveListHead.Flink, next = entry->Flink;
         entry != &tracker->ActiveListHead;
         entry = next, next = entry->Flink) {
        PHD_IRP_TRACK_ENTRY e = CONTAINING_RECORD(entry, HD_IRP_TRACK_ENTRY, ListEntry);
        if ((now - e->SubmitTimestamp) > (ULONGLONG)(HD_IRP_TIMEOUT_SEC * 1000)) {
            RemoveEntryList(&e->ListEntry);
            tracker->ActiveCount--;

            if (e->Irp) {
                e->Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
                e->Irp->IoStatus.Information = 0;
                IoCompleteRequest(e->Irp, IO_NO_INCREMENT);
            }
            ExFreePoolWithTag(e, 'TRPH');
        }
    }

    KeReleaseSpinLock(&tracker->Lock, oldIrql);

    if (tracker->ActiveCount > 0) {
        LARGE_INTEGER period;
        period.QuadPart = -10 * 1000 * 1000 * HD_IRP_SCAN_INTERVAL_SEC;
        KeSetTimerEx(&tracker->WatchdogTimer, period, HD_IRP_SCAN_INTERVAL_SEC * 1000, &tracker->WatchdogDpc);
    } else {
        tracker->TimerRunning = FALSE;
    }
}
