#include "blk_driver.h"

PVOID HdPoolAllocate(SIZE_T Size, ULONG Tag)
{
    PVOID ptr = ExAllocatePool2(POOL_FLAG_NON_PAGED, Size, Tag);
    return ptr;
}

VOID HdPoolFree(PVOID Ptr, ULONG Tag)
{
    if (Ptr) {
        ExFreePoolWithTag(Ptr, Tag);
    }
}

NTSTATUS HdPoolCheckQuota(PHD_BLK_EXTENSION BlkExt, SIZE_T Size)
{
    LONGLONG usage = BlkExt->NonPagedPoolUsage;
    LONGLONG quota = (LONGLONG)BlkExt->NonPagedPoolQuota;

    if ((usage + (LONGLONG)Size) > quota) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    InterlockedAdd64(&BlkExt->NonPagedPoolUsage, (LONGLONG)Size);
    return STATUS_SUCCESS;
}
