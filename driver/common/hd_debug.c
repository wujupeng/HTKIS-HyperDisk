#include "hd_common.h"

VOID HdDebugPrint(PCSTR Format, ...)
{
    va_list args;
    va_start(args, Format);
    vDbgPrintExWithPrefix("[HyperDisk] ", DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, Format, args);
    va_end(args);
}

PVOID HdAllocatePool(SIZE_T Size, ULONG Tag)
{
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, Size, Tag);
}

VOID HdFreePool(PVOID Ptr, ULONG Tag)
{
    if (Ptr) {
        ExFreePoolWithTag(Ptr, Tag);
    }
}

VOID HdInitializeSpinLock(PKSPIN_LOCK Lock)
{
    KeInitializeSpinLock(Lock);
}

VOID HdAcquireSpinLock(PKSPIN_LOCK Lock, PKIRQL OldIrql)
{
    KeAcquireSpinLock(Lock, OldIrql);
}

VOID HdReleaseSpinLock(PKSPIN_LOCK Lock, KIRQL OldIrql)
{
    KeReleaseSpinLock(Lock, OldIrql);
}
