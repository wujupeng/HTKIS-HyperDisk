#pragma once

#include <ntddk.h>
#include <storport.h>

#define HD_PORT_TAG        'PdHd'
#define HD_DISK_SIZE_GB    64
#define HD_SECTOR_SIZE     512
#define HD_MAX_TRANSFER    (128 * 1024)

typedef struct _HD_PORT_EXTENSION {
    ULONGLONG   DiskSize;
    ULONG       SectorSize;
    ULONG       TotalSectors;
    ULONG       Cylinders;
    ULONG       TracksPerCylinder;
    ULONG       SectorsPerTrack;
    UCHAR       ServerAddress[64];
    USHORT      ServerPort;
    BOOLEAN     Initialized;
} HD_PORT_EXTENSION, *PHD_PORT_EXTENSION;

ULONG DriverEntry(PVOID DriverObject, PVOID RegistryPath);

ULONG HdPortHwFindAdapter(PVOID DeviceExtension, PVOID HwContext, PVOID BusInformation,
    PCHAR ArgumentString, PPORT_CONFIGURATION_INFORMATION ConfigInfo, PBOOLEAN Again);
BOOLEAN HdPortHwInitialize(PVOID DeviceExtension);
BOOLEAN HdPortHwStartIo(PVOID DeviceExtension, PSCSI_REQUEST_BLOCK Srb);
ULONG HdPortHwResetBus(PVOID DeviceExtension, ULONG PathId);
ULONG HdPortHwAdapter(PVOID DeviceExtension);
ULONG HdPortHwStartDevice(PVOID DeviceExtension, PCHAR ArgumentString, PPORT_CONFIGURATION_INFORMATION ConfigInfo);

VOID HdPortHandleScsi(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
VOID HdPortHandleInquiry(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
VOID HdPortHandleReadCapacity(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
VOID HdPortHandleReadCapacity16(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
VOID HdPortHandleReadWrite(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
VOID HdPortHandleModeSense(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
VOID HdPortHandleRequestSense(PHD_PORT_EXTENSION Ext, PSCSI_REQUEST_BLOCK Srb);
