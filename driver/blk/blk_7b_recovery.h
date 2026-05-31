#pragma once

#include <ntifs.h>

#define HD_7B_MAX_RETRIES          3
#define HD_7B_RETRY_INTERVAL_MS    1000
#define HD_7B_FALLBACK_WAIT_SEC    30
#define HD_7B_RECOVERY_HALT        TRUE

typedef enum _HD_7B_STATE {
    Hd7bNormal = 0,
    Hd7bRetrying,
    Hd7bFallback,
    Hd7bRecovery,
    Hd7bHalted
} HD_7B_STATE;

typedef struct _HD_7B_RECOVERY {
    HD_7B_STATE    State;
    ULONG          RetryCount;
    ULONG          FallbackStep;
    LARGE_INTEGER  StateEntryTime;
    BOOLEAN        LocalMetaAvailable;
    BOOLEAN        NetworkAvailable;
    BOOLEAN        MetaServerReachable;
    BOOLEAN        BlockDeviceReady;
} HD_7B_RECOVERY, *PHD_7B_RECOVERY;

typedef VOID (*HD_7B_NOTIFY_CALLBACK)(PHD_7B_RECOVERY Recovery, PVOID Context);

NTSTATUS Hd7bInitialize(PHD_7B_RECOVERY Recovery);
NTSTATUS Hd7bEnterRecovery(PHD_7B_RECOVERY Recovery);
NTSTATUS Hd7bProcessRetry(PHD_7B_RECOVERY Recovery);
NTSTATUS Hd7bProcessFallback(PHD_7B_RECOVERY Recovery);
NTSTATUS Hd7bProcessRecovery(PHD_7B_RECOVERY Recovery);
const char* Hd7bStateName(HD_7B_STATE State);
