#include "cache_driver.h"
#include "../common/hd_serial.h"

HD_SERIAL_DEBUG g_SerialDebug;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    HdSerialInitialize(&g_SerialDebug);
    return HdCacheFilterSetup(DriverObject, RegistryPath);
}
