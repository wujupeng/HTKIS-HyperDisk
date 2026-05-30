#pragma once

#include <ntddk.h>
#include <wdm.h>
#include <fltkrnl.h>

#define HD_CACHE_DEVICE_NAME L"\\Device\\HyperCache"
#define HD_CACHE_ALTITUDE    L"380000"

#define HD_CACHE_L1_SIZE_DEFAULT_MB  2048
#define HD_CACHE_W_TINYLFU_WINDOW   256
#define HD_CACHE_BLOCK_SIZE         4096

typedef struct _HD_CACHE_EXTENSION {
    PDEVICE_OBJECT    FilterDeviceObject;
    UNICODE_STRING    FilterName;
    PVOID            L1RamCache;
    SIZE_T          L1CacheSize;
    ULONG           BlockSize;
    ULONG           CacheHitCount;
    ULONG           CacheMissCount;
    KSPIN_LOCK      CacheLock;
    BOOLEAN         IsStarted;
} HD_CACHE_EXTENSION, *PHD_CACHE_EXTENSION;

NTSTATUS HdCacheFilterSetup(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID HdCacheFilterUnload(PDRIVER_OBJECT DriverObject);

NTSTATUS HdCachePreRead(PFLT_INSTANCE Instance, PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext);
NTSTATUS HdCachePostRead(PFLT_INSTANCE Instance, PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);
NTSTATUS HdCachePreWrite(PFLT_INSTANCE Instance, PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext);

PVOID HdCacheL1Lookup(PHD_CACHE_EXTENSION CacheExt, ULONGLONG BlockOffset);
NTSTATUS HdCacheL1Insert(PHD_CACHE_EXTENSION CacheExt, ULONGLONG BlockOffset, PVOID Data, ULONG Size);
VOID HdCacheL1Invalidate(PHD_CACHE_EXTENSION CacheExt, ULONGLONG BlockOffset);
