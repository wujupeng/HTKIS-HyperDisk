#pragma once

#include <ntifs.h>
#include <fltKernel.h>

#define HD_OVERLAY_DEVICE_NAME L"\\Device\\HyperOverlay"
#define HD_OVERLAY_ALTITUDE   L"390000"

#define HD_OVERLAY_RAM_SIZE_DEFAULT_MB  4096
#define HD_OVERLAY_BLOCK_SIZE          4096
#define HD_OVERLAY_MAX_DIRTY_PAGES     1048576

typedef struct _HD_OVERLAY_DIRTY_PAGE {
    LIST_ENTRY  ListEntry;
    ULONGLONG   BlockOffset;
    PVOID       Data;
    ULONG       Size;
    BOOLEAN     IsDirty;
} HD_OVERLAY_DIRTY_PAGE, *PHD_OVERLAY_DIRTY_PAGE;

typedef struct _HD_OVERLAY_EXTENSION {
    PDEVICE_OBJECT    FilterDeviceObject;
    UNICODE_STRING    FilterName;
    PVOID            RamOverlayBase;
    SIZE_T          RamOverlaySize;
    ULONG           BlockSize;
    ULONG           DirtyPageCount;
    LIST_ENTRY      DirtyListHead;
    KSPIN_LOCK      DirtyListLock;
    BOOLEAN         WalEnabled;
    BOOLEAN         IsStarted;
} HD_OVERLAY_EXTENSION, *PHD_OVERLAY_EXTENSION;

NTSTATUS HdOverlayFilterSetup(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID HdOverlayFilterUnload(FLT_FILTER_UNLOAD_FLAGS Flags);

FLT_PREOP_CALLBACK_STATUS HdOverlayPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);
FLT_POSTOP_CALLBACK_STATUS HdOverlayPostWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);

NTSTATUS HdOverlayWriteRedirect(PHD_OVERLAY_EXTENSION OverlayExt, ULONGLONG BlockOffset, PVOID Data, ULONG Size);
NTSTATUS HdOverlaySyncDirtyPages(PHD_OVERLAY_EXTENSION OverlayExt);
VOID HdOverlayInvalidateAll(PHD_OVERLAY_EXTENSION OverlayExt);
