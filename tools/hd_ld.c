#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef LONG NTSTATUS;

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef VOID(NTAPI *pRtlInitUnicodeString)(PUNICODE_STRING, PCWSTR);
typedef NTSTATUS(NTAPI *pNtLoadDriver)(PUNICODE_STRING, PUNICODE_STRING);
typedef NTSTATUS(NTAPI *pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
typedef NTSTATUS(NTAPI *pNtSetSystemInformation)(ULONG, PVOID, ULONG);
typedef NTSTATUS(NTAPI *pNtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);

#define SE_DEBUG_PRIVILEGE 20
#define SystemCodeIntegrityInformation 0x67

typedef struct _SYSTEM_CODEINTEGRITY_INFORMATION {
    ULONG Length;
    ULONG CodeIntegrityOptions;
} SYSTEM_CODEINTEGRITY_INFORMATION, *PSYSTEM_CODEINTEGRITY_INFORMATION;

#define CODEINTEGRITY_OPTION_ENABLED 0x01
#define CODEINTEGRITY_OPTION_TESTSIGN 0x02
#define CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED 0x40000

static int relax_ci(void)
{
    HKEY hKey;
    DWORD val = 0;
    LONG err = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\CI\\Policy",
        0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
    if (err == ERROR_SUCCESS) {
        val = 0;
        RegSetValueExA(hKey, "Enabled", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        val = 3;
        RegSetValueExA(hKey, "TestSign", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        fprintf(stderr, "CI Policy reg: TestSign=3, Enabled=0\n");
    } else {
        fprintf(stderr, "CI Policy reg: err=%ld (may be ok)\n", err);
    }

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        fprintf(stderr, "CI relax: no ntdll\n");
        return -1;
    }

    pRtlAdjustPrivilege RtlAdjustPrivilege = (pRtlAdjustPrivilege)GetProcAddress(ntdll, "RtlAdjustPrivilege");
    if (RtlAdjustPrivilege) {
        BOOLEAN old;
        NTSTATUS st = RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old);
        fprintf(stderr, "SeDebugPrivilege: 0x%08lX\n", st);
    }

    pNtSetSystemInformation NtSetSystemInformation = (pNtSetSystemInformation)GetProcAddress(ntdll, "NtSetSystemInformation");
    pNtQuerySystemInformation NtQuerySystemInformation = (pNtQuerySystemInformation)GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (NtSetSystemInformation && NtQuerySystemInformation) {
        SYSTEM_CODEINTEGRITY_INFORMATION ci = {0};
        ci.Length = sizeof(ci);

        NTSTATUS st = NtQuerySystemInformation(SystemCodeIntegrityInformation, &ci, sizeof(ci), NULL);
        fprintf(stderr, "CI query: 0x%08lX options=0x%08lX\n", st, ci.CodeIntegrityOptions);

        if (NT_SUCCESS(st)) {
            ci.CodeIntegrityOptions &= ~CODEINTEGRITY_OPTION_ENABLED;
            ci.CodeIntegrityOptions |= CODEINTEGRITY_OPTION_TESTSIGN;

            st = NtSetSystemInformation(SystemCodeIntegrityInformation, &ci, sizeof(ci));
            fprintf(stderr, "CI set testsign: 0x%08lX\n", st);

            if (!NT_SUCCESS(st)) {
                fprintf(stderr, "CI runtime relax failed (0x%08lX), CI is locked by kernel\n", st);
                fprintf(stderr, "Need BCD testsigning=on BEFORE boot - modify BCD file\n");
            }
        }
    } else {
        fprintf(stderr, "NtSetSystemInformation not found\n");
    }

    return 0;
}

int load_driver(const char *name, const char *reg_path)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) { fprintf(stderr, "no ntdll\n"); return 1; }

    pRtlInitUnicodeString RtlInitUnicodeString = (pRtlInitUnicodeString)GetProcAddress(ntdll, "RtlInitUnicodeString");
    pNtLoadDriver NtLoadDriver = (pNtLoadDriver)GetProcAddress(ntdll, "NtLoadDriver");
    if (!NtLoadDriver || !RtlInitUnicodeString) { fprintf(stderr, "no NtLoadDriver\n"); return 1; }

    wchar_t wname[512], wreg[512];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, 512);
    MultiByteToWideChar(CP_ACP, 0, reg_path, -1, wreg, 512);

    UNICODE_STRING svcName, regPath;
    RtlInitUnicodeString(&svcName, wname);
    RtlInitUnicodeString(&regPath, wreg);

    NTSTATUS status = NtLoadDriver(&svcName, &regPath);
    fprintf(stderr, "NtLoadDriver %s = 0x%08lX\n", name, status);
    return (status != 0) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: hd_ld.exe ServiceRegPath1 [ServiceRegPath2 ...]\n");
        fprintf(stderr, "  Each ServiceRegPath = \\Registry\\Machine\\System\\CurrentControlSet\\Services\\<name>\n");
        return 1;
    }
    relax_ci();
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int r = load_driver(argv[i], argv[i]);
        if (r) rc = r;
    }
    return rc;
}
