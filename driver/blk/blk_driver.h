#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <ntdddisk.h>
#include "hd_driver_ioctl.h"

#define HD_BLK_DEVICE_NAME L"\\Device\\HyperDiskBlk%d"
#define HD_BLK_SYM_LINK   L"\\DosDevices\\HyperDiskBlk%d"

#define HD_IRP_TIMEOUT_SEC      60
#define HD_IRP_SCAN_INTERVAL_SEC 30
#define HD_DEADLOCK_TIMEOUT_SEC  5
#define HD_NONPAGED_POOL_RATIO   80
#define HD_MAX_PENDING_IRPS      65536

typedef struct _HD_BLK_EXTENSION {
    PDEVICE_OBJECT  DeviceObject;
    ULONG           ChildId;
    ULONGLONG       ImageId;
    ULONGLONG       DiskSize;
    ULONG           BlockSize;
    ULONG           BlockCount;
    UCHAR           LayerId;
    BOOLEAN         IsStarted;
    LONG            OpenHandleCount;
    LONG            PendingIoCount;
    KEVENT          IoEvent;
    KEVENT          ShutdownEvent;
    ERESOURCE       FsRtlHeaderLock;
    ERESOURCE       CcFlushLock;
    ERESOURCE       MmModWriteLock;
    KSPIN_LOCK      DriverSpinLock;
    KSPIN_LOCK      CacheSharedMemLock;
    ULONGLONG       NonPagedPoolQuota;
    volatile LONGLONG NonPagedPoolUsage;
} HD_BLK_EXTENSION, *PHD_BLK_EXTENSION;

typedef struct _HD_IRP_TRACK_ENTRY {
    LIST_ENTRY  ListEntry;
    PIRP        Irp;
    ULONGLONG   SubmitTimestamp;
    UCHAR       MajorFunction;
    UCHAR       MinorFunction;
    ULONG       IoSize;
} HD_IRP_TRACK_ENTRY, *PHD_IRP_TRACK_ENTRY;

typedef struct _HD_IRP_TRACKER {
    KSPIN_LOCK      Lock;
    LIST_ENTRY      ActiveListHead;
    ULONG           ActiveCount;
    KDPC            WatchdogDpc;
    KTIMER          WatchdogTimer;
    BOOLEAN         TimerRunning;
} HD_IRP_TRACKER, *PHD_IRP_TRACKER;

NTSTATUS HdBlkDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchFlushBuffers(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchShutdown(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchQueryInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchSetInformation(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkSubmitBlockRequest(PHD_BLK_EXTENSION BlkExt, PHD_BLOCK_REQUEST Request);

VOID HdIrpTrackerInit(PHD_IRP_TRACKER Tracker);
VOID HdIrpTrackerDestroy(PHD_IRP_TRACKER Tracker);
VOID HdIrpTrackerSubmit(PHD_IRP_TRACKER Tracker, PIRP Irp, UCHAR MajorFn, UCHAR MinorFn);
VOID HdIrpTrackerComplete(PHD_IRP_TRACKER Tracker, PIRP Irp);
VOID HdIrpTrackerWatchdog(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);

BOOLEAN HdBlkFastIoRead(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length, BOOLEAN Wait, ULONG LockKey, PVOID Buffer, PIO_STATUS_BLOCK IoStatus, PDEVICE_OBJECT DeviceObject);
BOOLEAN HdBlkFastIoWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length, BOOLEAN Wait, ULONG LockKey, PVOID Buffer, PIO_STATUS_BLOCK IoStatus, PDEVICE_OBJECT DeviceObject);
BOOLEAN HdBlkFastIoQueryBasicInfo(PFILE_OBJECT FileObject, BOOLEAN Wait, PFILE_BASIC_INFORMATION Buffer, PIO_STATUS_BLOCK IoStatus, PDEVICE_OBJECT DeviceObject);
BOOLEAN HdBlkFastIoQueryStandardInfo(PFILE_OBJECT FileObject, BOOLEAN Wait, PFILE_STANDARD_INFORMATION Buffer, PIO_STATUS_BLOCK IoStatus, PDEVICE_OBJECT DeviceObject);
BOOLEAN HdBlkFastIoDeviceControl(PFILE_OBJECT FileObject, BOOLEAN Wait, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG IoControlCode, PIO_STATUS_BLOCK IoStatus, PDEVICE_OBJECT DeviceObject);

NTSTATUS HdBlkAcquireForCcFlush(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject);
NTSTATUS HdBlkReleaseForCcFlush(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject);
NTSTATUS HdBlkAcquireForModWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER EndingOffset, PERESOURCE Resource, PDEVICE_OBJECT DeviceObject);
NTSTATUS HdBlkReleaseForModWrite(PFILE_OBJECT FileObject, PERESOURCE Resource, PDEVICE_OBJECT DeviceObject);
NTSTATUS HdBlkAcquireFileForNtCreateSection(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject);
NTSTATUS HdBlkReleaseFileForNtCreateSection(PFILE_OBJECT FileObject, PDEVICE_OBJECT DeviceObject);

PVOID HdPoolAllocate(SIZE_T Size, ULONG Tag);
VOID   HdPoolFree(PVOID Ptr, ULONG Tag);
NTSTATUS HdPoolCheckQuota(PHD_BLK_EXTENSION BlkExt, SIZE_T Size);
