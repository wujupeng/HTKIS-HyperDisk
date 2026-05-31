#include "hd_common.h"

VOID HdDebugPrint(PCSTR Format, ...)
{
    va_list args;
    va_start(args, Format);
    vDbgPrintExWithPrefix("[HyperDisk] ", DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, Format, args);
    va_end(args);
}
