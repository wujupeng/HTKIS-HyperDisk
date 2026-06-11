#pragma once

#include <ntddk.h>
#include <ntdddisk.h>
#include <ntddscsi.h>
#include <mountmgr.h>
#include <mountdev.h>

#define HD_BLK_DEVICE_NAME L"\\Device\\HyperDiskBlk0"
#define HD_BLK_SYM_LINK   L"\\DosDevices\\PhysicalDrive1"
#define HD_BLK_POOL_TAG   'KdBH'

#define HD_DISK_SIZE_GB    64
#define HD_SECTOR_SIZE     512
#define HD_DISK_SIZE       ((ULONGLONG)HD_DISK_SIZE_GB * 1024 * 1024 * 1024)
#define HD_TOTAL_SECTORS   (HD_DISK_SIZE / HD_SECTOR_SIZE)
#define HD_CYLINDERS       (HD_TOTAL_SECTORS / (63 * 255))

typedef struct _HD_BLK_DEVICE {
    PDEVICE_OBJECT  DeviceObject;
    ULONGLONG       DiskSize;
    ULONG           SectorSize;
    ULONG           TotalSectors;
    ULONG           Cylinders;
    ULONG           TracksPerCylinder;
    ULONG           SectorsPerTrack;
    BOOLEAN         Initialized;
    UNICODE_STRING  InterfaceName;
    BOOLEAN         InterfaceEnabled;
} HD_BLK_DEVICE, *PHD_BLK_DEVICE;

DRIVER_UNLOAD HdBlkUnload;

NTSTATUS HdBlkDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkNotifyMountManager(PDEVICE_OBJECT DeviceObject);
