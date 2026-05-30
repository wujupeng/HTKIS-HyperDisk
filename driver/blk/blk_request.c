#include "blk_driver.h"

NTSTATUS HdBlkSubmitBlockRequest(PHD_BLK_EXTENSION BlkExt, PHD_BLOCK_REQUEST Request)
{
    UNREFERENCED_PARAMETER(BlkExt);
    UNREFERENCED_PARAMETER(Request);

    return STATUS_SUCCESS;
}
