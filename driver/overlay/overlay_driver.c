#include "overlay_driver.h"
#include "../common/hd_serial.h"

extern HD_SERIAL_DEBUG g_SerialDebug;

static HD_OVERLAY_EXTENSION g_OverlayExt;

static FLT_OPERATION_REGISTRATION g_Callbacks[] = {
    { IRP_MJ_WRITE, 0, HdOverlayPreWrite, HdOverlayPostWrite, NULL },
    { (UCHAR)-1, 0, NULL, NULL, NULL }
};

static FLT_CONTEXT_REGISTRATION g_ContextRegistration[] = {
    { FLT_CONTEXT_END }
};

static FLT_REGISTRATION g_FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    g_ContextRegistration,
    g_Callbacks,
    HdOverlayFilterUnload,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static PFLT_FILTER g_FilterHandle = NULL;

NTSTATUS HdOverlayFilterSetup(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    UNICODE_STRING altitude;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlZeroMemory(&g_OverlayExt, sizeof(HD_OVERLAY_EXTENSION));
    KeInitializeSpinLock(&g_OverlayExt.DirtyListLock);
    InitializeListHead(&g_OverlayExt.DirtyListHead);
    g_OverlayExt.BlockSize = HD_OVERLAY_BLOCK_SIZE;
    g_OverlayExt.WalEnabled = FALSE;

    g_OverlayExt.RamOverlaySize = (SIZE_T)HD_OVERLAY_RAM_SIZE_DEFAULT_MB * 1024 * 1024;
    g_OverlayExt.RamOverlayBase = ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        g_OverlayExt.RamOverlaySize,
        'OVHD'
    );

    RtlInitUnicodeString(&altitude, HD_OVERLAY_ALTITUDE);

    status = FltRegisterFilter(DriverObject, &g_FilterRegistration, &g_FilterHandle);
    if (!NT_SUCCESS(status)) {
        HdSerialWriteString(&g_SerialDebug, "[HDx:OVERLAY_LOADED] FltRegisterFilter failed\r\n");
        return status;
    }

    status = FltStartFiltering(g_FilterHandle);
    if (!NT_SUCCESS(status)) {
        HdSerialWriteString(&g_SerialDebug, "[HDx:OVERLAY_LOADED] FltStartFiltering failed\r\n");
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
        return status;
    }

    g_OverlayExt.IsStarted = TRUE;
    HdSerialWriteString(&g_SerialDebug, "[HDx:OVERLAY_LOADED] HyperOverlay MiniFilter started\r\n");

    return STATUS_SUCCESS;
}

VOID HdOverlayFilterUnload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);

    if (g_FilterHandle) {
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
    }

    if (g_OverlayExt.RamOverlayBase) {
        ExFreePoolWithTag(g_OverlayExt.RamOverlayBase, 'OVHD');
        g_OverlayExt.RamOverlayBase = NULL;
    }

    g_OverlayExt.IsStarted = FALSE;
    HdSerialWriteString(&g_SerialDebug, "[HDx:OVERLAY_LOADED] HyperOverlay unloaded\r\n");
}

FLT_PREOP_CALLBACK_STATUS HdOverlayPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (!g_OverlayExt.IsStarted) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS HdOverlayPostWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS HdOverlayWriteRedirect(PHD_OVERLAY_EXTENSION OverlayExt, ULONGLONG BlockOffset, PVOID Data, ULONG Size)
{
    KIRQL oldIrql;

    if (!OverlayExt->RamOverlayBase) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (OverlayExt->DirtyPageCount >= HD_OVERLAY_MAX_DIRTY_PAGES) {
        HdOverlaySyncDirtyPages(OverlayExt);
    }

    PHD_OVERLAY_DIRTY_PAGE page = (PHD_OVERLAY_DIRTY_PAGE)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(HD_OVERLAY_DIRTY_PAGE),
        'DRTY'
    );
    if (!page) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    page->BlockOffset = BlockOffset;
    page->Data = ExAllocatePool2(POOL_FLAG_NON_PAGED, Size, 'DRTD');
    if (!page->Data) {
        ExFreePoolWithTag(page, 'DRTY');
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory(page->Data, Data, Size);
    page->Size = Size;
    page->IsDirty = TRUE;

    KeAcquireSpinLock(&OverlayExt->DirtyListLock, &oldIrql);
    InsertTailList(&OverlayExt->DirtyListHead, &page->ListEntry);
    OverlayExt->DirtyPageCount++;
    KeReleaseSpinLock(&OverlayExt->DirtyListLock, oldIrql);

    return STATUS_SUCCESS;
}

NTSTATUS HdOverlaySyncDirtyPages(PHD_OVERLAY_EXTENSION OverlayExt)
{
    KIRQL oldIrql;
    PLIST_ENTRY entry;
    PHD_OVERLAY_DIRTY_PAGE page;
    ULONG synced = 0;

    KeAcquireSpinLock(&OverlayExt->DirtyListLock, &oldIrql);
    while (!IsListEmpty(&OverlayExt->DirtyListHead)) {
        entry = RemoveHeadList(&OverlayExt->DirtyListHead);
        page = CONTAINING_RECORD(entry, HD_OVERLAY_DIRTY_PAGE, ListEntry);
        if (page->Data) {
            ExFreePoolWithTag(page->Data, 'DRTD');
        }
        ExFreePoolWithTag(page, 'DRTY');
        OverlayExt->DirtyPageCount--;
        synced++;
    }
    KeReleaseSpinLock(&OverlayExt->DirtyListLock, oldIrql);

    UNREFERENCED_PARAMETER(OverlayExt);
    return STATUS_SUCCESS;
}

VOID HdOverlayInvalidateAll(PHD_OVERLAY_EXTENSION OverlayExt)
{
    HdOverlaySyncDirtyPages(OverlayExt);
}
