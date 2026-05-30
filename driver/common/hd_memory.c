#include "hd_common.h"

PVOID HdAllocatePool(SIZE_T Size, ULONG Tag) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, Size, Tag); }
VOID  HdFreePool(PVOID Ptr, ULONG Tag) { if (Ptr) ExFreePoolWithTag(Ptr, Tag); }
