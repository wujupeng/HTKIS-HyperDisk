#pragma once

#include <ntifs.h>
#include <ntdddisk.h>
#include <ntddscsi.h>
#include <mountdev.h>

#define HD_BUS_DEVICE_NAME L"\\Device\\HyperDiskBus"
#define HD_BUS_SYM_LINK   L"\\DosDevices\\HyperDiskBus"

#define HD_POOL_TAG        'SdBH'

#define HD_DISK_SIZE_GB    64
#define HD_SECTOR_SIZE     512
#define HD_DISK_SIZE       ((ULONGLONG)HD_DISK_SIZE_GB * 1024 * 1024 * 1024)
#define HD_TOTAL_SECTORS   (HD_DISK_SIZE / HD_SECTOR_SIZE)
#define HD_CYLINDERS       (HD_TOTAL_SECTORS / (63 * 255))

typedef struct _HD_BUS_EXTENSION {
    PDEVICE_OBJECT  BusDeviceObject;
    UNICODE_STRING  BusDeviceName;
    UNICODE_STRING  BusSymLinkName;
    ULONG           ChildCount;
    LIST_ENTRY      ChildListHead;
    KSPIN_LOCK      ChildListLock;
} HD_BUS_EXTENSION, *PHD_BUS_EXTENSION;

typedef struct _HD_CHILD_DEVICE {
    LIST_ENTRY      ListEntry;
    ULONG           ChildId;
    PDEVICE_OBJECT  ChildDeviceObject;
    ULONGLONG       DiskSize;
    ULONG           SectorSize;
    ULONG           TotalSectors;
    ULONG           Cylinders;
    ULONG           TracksPerCylinder;
    ULONG           SectorsPerTrack;
    UNICODE_STRING  InterfaceName;
    BOOLEAN         InterfaceRegistered;
    BOOLEAN         Started;
} HD_CHILD_DEVICE, *PHD_CHILD_DEVICE;

DRIVER_UNLOAD HdBusUnload;

NTSTATUS HdBusDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBusDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBusDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBusDispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBusDispatchSystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
