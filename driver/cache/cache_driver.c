#include "cache_driver.h"
#include "../common/hd_serial.h"

extern HD_SERIAL_DEBUG g_SerialDebug;

static HD_CACHE_EXTENSION g_CacheExt;

static FLT_OPERATION_REGISTRATION g_Callbacks[] = {
    { IRP_MJ_READ, 0, HdCachePreRead, HdCachePostRead, NULL },
    { IRP_MJ_WRITE, 0, HdCachePreWrite, NULL, NULL },
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
    HdCacheFilterUnload,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static PFLT_FILTER g_FilterHandle = NULL;

NTSTATUS HdCacheFilterSetup(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    UNICODE_STRING altitude;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlZeroMemory(&g_CacheExt, sizeof(HD_CACHE_EXTENSION));
    KeInitializeSpinLock(&g_CacheExt.CacheLock);
    g_CacheExt.BlockSize = HD_CACHE_BLOCK_SIZE;

    RtlInitUnicodeString(&altitude, HD_CACHE_ALTITUDE);

    status = FltRegisterFilter(DriverObject, &g_FilterRegistration, &g_FilterHandle);
    if (!NT_SUCCESS(status)) {
        HdSerialWriteString(&g_SerialDebug, "[HDx:CACHE_LOADED] FltRegisterFilter failed\r\n");
        return status;
    }

    status = FltStartFiltering(g_FilterHandle);
    if (!NT_SUCCESS(status)) {
        HdSerialWriteString(&g_SerialDebug, "[HDx:CACHE_LOADED] FltStartFiltering failed\r\n");
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
        return status;
    }

    g_CacheExt.IsStarted = TRUE;
    HdSerialWriteString(&g_SerialDebug, "[HDx:CACHE_LOADED] HyperCache MiniFilter started\r\n");

    return STATUS_SUCCESS;
}

VOID HdCacheFilterUnload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);

    if (g_FilterHandle) {
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
    }

    g_CacheExt.IsStarted = FALSE;
    HdSerialWriteString(&g_SerialDebug, "[HDx:CACHE_LOADED] HyperCache unloaded\r\n");
}

FLT_PREOP_CALLBACK_STATUS HdCachePreRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);

    if (!g_CacheExt.IsStarted) {
        return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    }

    PVOID cached = HdCacheL1Lookup(&g_CacheExt, 0);
    if (cached) {
        g_CacheExt.CacheHitCount++;
        return FLT_PREOP_COMPLETE;
    }

    g_CacheExt.CacheMissCount++;
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS HdCachePostRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS HdCachePreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

PVOID HdCacheL1Lookup(PHD_CACHE_EXTENSION CacheExt, ULONGLONG BlockOffset)
{
    KIRQL oldIrql;
    PVOID result = NULL;

    KeAcquireSpinLock(&CacheExt->CacheLock, &oldIrql);
    if (CacheExt->L1RamCache && BlockOffset == 0) {
        result = CacheExt->L1RamCache;
    }
    KeReleaseSpinLock(&CacheExt->CacheLock, oldIrql);

    return result;
}

NTSTATUS HdCacheL1Insert(PHD_CACHE_EXTENSION CacheExt, ULONGLONG BlockOffset, PVOID Data, ULONG Size)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&CacheExt->CacheLock, &oldIrql);
    if (!CacheExt->L1RamCache && Size > 0) {
        CacheExt->L1RamCache = ExAllocatePool2(POOL_FLAG_NON_PAGED, Size, 'CACH');
        if (CacheExt->L1RamCache) {
            RtlCopyMemory(CacheExt->L1RamCache, Data, Size);
        }
    }
    KeReleaseSpinLock(&CacheExt->CacheLock, oldIrql);

    UNREFERENCED_PARAMETER(BlockOffset);
    return CacheExt->L1RamCache ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

VOID HdCacheL1Invalidate(PHD_CACHE_EXTENSION CacheExt, ULONGLONG BlockOffset)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&CacheExt->CacheLock, &oldIrql);
    if (CacheExt->L1RamCache) {
        ExFreePoolWithTag(CacheExt->L1RamCache, 'CACH');
        CacheExt->L1RamCache = NULL;
    }
    KeReleaseSpinLock(&CacheExt->CacheLock, oldIrql);

    UNREFERENCED_PARAMETER(BlockOffset);
}
