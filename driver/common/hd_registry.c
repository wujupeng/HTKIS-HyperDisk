#include "hd_common.h"

NTSTATUS HdReadRegistryString(PUNICODE_STRING RegistryPath, PWSTR ValueName, PUNICODE_STRING Result)
{
    HANDLE keyHandle;
    NTSTATUS status;
    UNICODE_STRING name;
    ULONG resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION info;

    RtlInitUnicodeString(&name, ValueName);

    OBJECT_ATTRIBUTES objAttrs;
    InitializeObjectAttributes(&objAttrs, RegistryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwOpenKey(&keyHandle, KEY_READ, &objAttrs);

    if (!NT_SUCCESS(status)) return status;

    status = ZwQueryValueKey(keyHandle, &name, KeyValuePartialInformation, NULL, 0, &resultLength);
    if (status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL) {
        ZwClose(keyHandle);
        return status;
    }

    info = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_NON_PAGED, resultLength, 'vRdH');
    if (!info) { ZwClose(keyHandle); return STATUS_INSUFFICIENT_RESOURCES; }

    status = ZwQueryValueKey(keyHandle, &name, KeyValuePartialInformation, info, resultLength, &resultLength);
    ZwClose(keyHandle);

    if (NT_SUCCESS(status) && info->Type == REG_SZ) {
        Result->Buffer = (PWCH)info->Data;
        Result->Length = (USHORT)(info->DataLength - sizeof(WCHAR));
        Result->MaximumLength = (USHORT)info->DataLength;
    } else {
        ExFreePoolWithTag(info, 'vRdH');
    }

    return status;
}

NTSTATUS HdReadRegistryDword(PUNICODE_STRING RegistryPath, PWSTR ValueName, PULONG Result)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(Result);
    return STATUS_NOT_IMPLEMENTED;
}
