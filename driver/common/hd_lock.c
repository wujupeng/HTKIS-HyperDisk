#include "hd_common.h"

VOID HdInitializeSpinLock(PKSPIN_LOCK Lock) { KeInitializeSpinLock(Lock); }
VOID HdAcquireSpinLock(PKSPIN_LOCK Lock, PKIRQL OldIrql) { KeAcquireSpinLock(Lock, OldIrql); }
VOID HdReleaseSpinLock(PKSPIN_LOCK Lock, KIRQL OldIrql) { KeReleaseSpinLock(Lock, OldIrql); }
