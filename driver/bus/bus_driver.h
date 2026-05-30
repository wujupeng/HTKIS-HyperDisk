#include <ntddk.h>
#include <wdm.h>
#include "hd_driver_ioctl.h"

#define HD_BUS_DEVICE_NAME L"\\Device\\HyperDiskBus"
#define HD_BUS_SYM_LINK   L"\\DosDevices\\HyperDiskBus"

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
    UNICODE_STRING  DeviceName;
    PDEVICE_OBJECT  ChildDeviceObject;
    HD_DEVICE_CONFIG Config;
} HD_CHILD_DEVICE, *PHD_CHILD_DEVICE;

DRIVER_UNLOAD HdBusUnload;

extern PHD_BUS_EXTENSION g_BusExtension;

NTSTATUS HdBusCreateChildDevice(PHD_BUS_EXTENSION BusExt, PHD_DEVICE_CONFIG Config);
NTSTATUS HdBusRemoveChildDevice(PHD_BUS_EXTENSION BusExt, ULONG ChildId);
