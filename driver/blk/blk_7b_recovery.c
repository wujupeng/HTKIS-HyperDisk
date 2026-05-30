#include "blk_7b_recovery.h"
#include "hd_serial.h"

extern HD_SERIAL_DEBUG g_SerialDebug;

NTSTATUS Hd7bInitialize(PHD_7B_RECOVERY Recovery)
{
    Recovery->State = Hd7bNormal;
    Recovery->RetryCount = 0;
    Recovery->FallbackStep = 0;
    Recovery->LocalMetaAvailable = FALSE;
    Recovery->NetworkAvailable = FALSE;
    Recovery->MetaServerReachable = FALSE;
    Recovery->BlockDeviceReady = FALSE;
    KeQuerySystemTime(&Recovery->StateEntryTime);
    return STATUS_SUCCESS;
}

NTSTATUS Hd7bEnterRecovery(PHD_7B_RECOVERY Recovery)
{
    Recovery->State = Hd7bRetrying;
    Recovery->RetryCount = 0;
    KeQuerySystemTime(&Recovery->StateEntryTime);

    HdSerialWriteString(&g_SerialDebug, "[0x7B] Enter recovery: INACCESSIBLE_BOOT_DEVICE\r\n");
    HdSerialWriteFormat(&g_SerialDebug, "[0x7B] Network=%d Meta=%d Block=%d\r\n",
        Recovery->NetworkAvailable,
        Recovery->MetaServerReachable,
        Recovery->BlockDeviceReady);

    return Hd7bProcessRetry(Recovery);
}

NTSTATUS Hd7bProcessRetry(PHD_7B_RECOVERY Recovery)
{
    if (Recovery->RetryCount >= HD_7B_MAX_RETRIES) {
        Recovery->State = Hd7bFallback;
        Recovery->FallbackStep = 0;
        KeQuerySystemTime(&Recovery->StateEntryTime);
        HdSerialWriteString(&g_SerialDebug, "[0x7B] Retry exhausted, entering fallback\r\n");
        return Hd7bProcessFallback(Recovery);
    }

    Recovery->RetryCount++;
    HdSerialWriteFormat(&g_SerialDebug, "[0x7B] Retry %d/%d\r\n",
        Recovery->RetryCount, HD_7B_MAX_RETRIES);

    LARGE_INTEGER delay;
    delay.QuadPart = -10 * 1000 * HD_7B_RETRY_INTERVAL_MS;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);

    if (Recovery->BlockDeviceReady && Recovery->MetaServerReachable) {
        Recovery->State = Hd7bNormal;
        HdSerialWriteString(&g_SerialDebug, "[0x7B] Retry succeeded, resuming normal\r\n");
        return STATUS_SUCCESS;
    }

    return Hd7bProcessRetry(Recovery);
}

NTSTATUS Hd7bProcessFallback(PHD_7B_RECOVERY Recovery)
{
    switch (Recovery->FallbackStep) {
    case 0:
        HdSerialWriteString(&g_SerialDebug, "[0x7B] Fallback step 0: Try secondary server\r\n");
        Recovery->FallbackStep++;
        break;
    case 1:
        HdSerialWriteString(&g_SerialDebug, "[0x7B] Fallback step 1: Use local boot.meta cache\r\n");
        if (Recovery->LocalMetaAvailable) {
            HdSerialWriteString(&g_SerialDebug, "[0x7B] Local meta available, attempting boot\r\n");
            Recovery->State = Hd7bNormal;
            return STATUS_SUCCESS;
        }
        Recovery->FallbackStep++;
        break;
    case 2:
        HdSerialWriteString(&g_SerialDebug, "[0x7B] Fallback step 2: Wait for network\r\n");
        {
            LARGE_INTEGER wait;
            wait.QuadPart = -10 * 1000 * 1000 * HD_7B_FALLBACK_WAIT_SEC;
            KeDelayExecutionThread(KernelMode, FALSE, &wait);
        }
        if (Recovery->NetworkAvailable) {
            Recovery->State = Hd7bRetrying;
            Recovery->RetryCount = 0;
            return Hd7bProcessRetry(Recovery);
        }
        Recovery->FallbackStep++;
        break;
    case 3:
        HdSerialWriteString(&g_SerialDebug, "[0x7B] Fallback step 3: Wait for block device\r\n");
        Recovery->FallbackStep++;
        break;
    default:
        Recovery->State = Hd7bRecovery;
        return Hd7bProcessRecovery(Recovery);
    }

    return STATUS_DEVICE_NOT_READY;
}

NTSTATUS Hd7bProcessRecovery(PHD_7B_RECOVERY Recovery)
{
    Recovery->State = Hd7bHalted;

    HdSerialWriteString(&g_SerialDebug, "\r\n[0x7B] === RECOVERY HALTED ===\r\n");
    HdSerialWriteString(&g_SerialDebug, "[0x7B] System cannot boot. Diagnostic info:\r\n");
    HdSerialWriteFormat(&g_SerialDebug, "[0x7B] Network: %d\r\n", Recovery->NetworkAvailable);
    HdSerialWriteFormat(&g_SerialDebug, "[0x7B] MetaServer: %d\r\n", Recovery->MetaServerReachable);
    HdSerialWriteFormat(&g_SerialDebug, "[0x7B] BlockDev: %d\r\n", Recovery->BlockDeviceReady);
    HdSerialWriteFormat(&g_SerialDebug, "[0x7B] LocalMeta: %d\r\n", Recovery->LocalMetaAvailable);
    HdSerialWriteString(&g_SerialDebug, "[0x7B] Awaiting manual intervention...\r\n");

    while (HD_7B_RECOVERY_HALT) {
        LARGE_INTEGER delay;
        delay.QuadPart = -10 * 1000 * 1000;
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    return STATUS_DEVICE_NOT_READY;
}

const char* Hd7bStateName(HD_7B_STATE State)
{
    static const char* names[] = {"Normal", "Retrying", "Fallback", "Recovery", "Halted"};
    return names[State];
}
