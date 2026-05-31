#pragma once

#include <ntifs.h>

VOID HdDebugPrint(PCSTR Format, ...);
PVOID HdAllocatePool(SIZE_T Size, ULONG Tag);
VOID  HdFreePool(PVOID Ptr, ULONG Tag);
VOID  HdInitializeSpinLock(PKSPIN_LOCK Lock);
VOID  HdAcquireSpinLock(PKSPIN_LOCK Lock, PKIRQL OldIrql);
VOID  HdReleaseSpinLock(PKSPIN_LOCK Lock, KIRQL OldIrql);
NTSTATUS HdReadRegistryString(PUNICODE_STRING RegistryPath, PWSTR ValueName, PUNICODE_STRING Result);
NTSTATUS HdReadRegistryDword(PUNICODE_STRING RegistryPath, PWSTR ValueName, PULONG Result);
