#pragma once

#include <ntddk.h>
#include <ntdddisk.h>
#include <ntddscsi.h>
#include <mountdev.h>

#define HD_BLK_POOL_TAG   'KdBH'

#define HD_DISK_SIZE_GB    64
#define HD_SECTOR_SIZE     512
#define HD_DISK_SIZE       ((ULONGLONG)HD_DISK_SIZE_GB * 1024 * 1024 * 1024)
#define HD_TOTAL_SECTORS   (HD_DISK_SIZE / HD_SECTOR_SIZE)
#define HD_CYLINDERS       (HD_TOTAL_SECTORS / (63 * 255))

typedef struct _HD_BLK_DEVICE {
    PDEVICE_OBJECT  DeviceObject;
    PDEVICE_OBJECT  LowerDevice;
    ULONGLONG       DiskSize;
    ULONG           SectorSize;
    ULONG           TotalSectors;
    ULONG           Cylinders;
    ULONG           TracksPerCylinder;
    ULONG           SectorsPerTrack;
    BOOLEAN         Started;
    UNICODE_STRING  InterfaceName;
    BOOLEAN         InterfaceRegistered;
} HD_BLK_DEVICE, *PHD_BLK_DEVICE;

NTSTATUS HdBlkAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS HdBlkDispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HdBlkDispatchSystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
DRIVER_UNLOAD HdBlkUnload;
