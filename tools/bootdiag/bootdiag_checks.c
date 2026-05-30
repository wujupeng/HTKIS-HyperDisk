#include "bootdiag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <inaddr.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

static int read_device_sector(BOOTDIAG_CONTEXT* ctx, uint64_t lba, uint8_t* buf, uint32_t sector_size) {
#ifdef _WIN32
    HANDLE hDev = CreateFileA(ctx->device_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL);
    if (hDev == INVALID_HANDLE_VALUE) {
        return -1;
    }

    LARGE_INTEGER offset;
    offset.QuadPart = (LONGLONG)(lba * sector_size);
    if (!SetFilePointerEx(hDev, offset, NULL, FILE_BEGIN)) {
        CloseHandle(hDev);
        return -2;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hDev, buf, sector_size, &bytesRead, NULL) || bytesRead != sector_size) {
        CloseHandle(hDev);
        return -3;
    }

    CloseHandle(hDev);
    return 0;
#else
    int fd = open(ctx->device_path, O_RDONLY | O_BINARY);
    if (fd < 0) return -1;

    off_t off = (off_t)(lba * sector_size);
    if (lseek(fd, off, SEEK_SET) < 0) {
        close(fd);
        return -2;
    }

    ssize_t rd = read(fd, buf, sector_size);
    close(fd);
    if (rd != (ssize_t)sector_size) return -3;
    return 0;
#endif
}

static int read_file_bytes(BOOTDIAG_CONTEXT* ctx, const char* path, uint64_t offset, uint8_t* buf, uint32_t size) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)offset;
    if (offset > 0 && !SetFilePointerEx(hFile, li, NULL, FILE_BEGIN)) {
        CloseHandle(hFile);
        return -2;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buf, size, &bytesRead, NULL) || bytesRead != size) {
        CloseHandle(hFile);
        return -3;
    }

    CloseHandle(hFile);
    return 0;
#else
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (offset > 0) fseek(f, (long)offset, SEEK_SET);
    size_t rd = fread(buf, 1, size, f);
    fclose(f);
    if (rd != size) return -3;
    return 0;
#endif
}

static void format_gpt_guid(const uint8_t* guid, char* out) {
    uint32_t p1 = guid[0] | (guid[1] << 8) | (guid[2] << 16) | (guid[3] << 24);
    uint16_t p2 = guid[4] | (guid[5] << 8);
    uint16_t p3 = guid[6] | (guid[7] << 8);
    sprintf(out, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            p1, p2, p3, guid[8], guid[9], guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
}

uint32_t bootdiag_v1_gpt(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
    uint8_t sector[512];
    int rc;
    uint32_t partition_count = 0;

    rc = read_device_sector(ctx, 0, sector, 512);
    if (rc != 0) {
        result->error_code = 1001;
        snprintf(result->detail, sizeof(result->detail), "Cannot read LBA 0 (protective MBR): error %d", rc);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint16_t mbr_sig = (uint16_t)(sector[510] | (sector[511] << 8));
    if (mbr_sig != 0x55AA) {
        result->error_code = 1002;
        snprintf(result->detail, sizeof(result->detail), "MBR signature invalid: 0x%04X (expected 0x55AA)", mbr_sig);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (sector[446] != 0xEE) {
        result->error_code = 1003;
        snprintf(result->detail, sizeof(result->detail), "Partition type=0x%02X (expected 0xEE GPT protective)", sector[446]);
        return BOOTDIAG_RESULT_FAIL;
    }

    rc = read_device_sector(ctx, 1, sector, 512);
    if (rc != 0) {
        result->error_code = 1010;
        snprintf(result->detail, sizeof(result->detail), "Cannot read LBA 1 (GPT header): error %d", rc);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (memcmp(sector, "EFI PART", 8) != 0) {
        result->error_code = 1011;
        snprintf(result->detail, sizeof(result->detail), "GPT signature mismatch: %.8s", sector);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t header_size = sector[12] | (sector[13] << 8) | (sector[14] << 16) | (sector[15] << 24);
    if (header_size != 92) {
        result->error_code = 1012;
        snprintf(result->detail, sizeof(result->detail), "HeaderSize=%u (expected 92)", header_size);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint64_t my_lba = 0;
    for (int i = 0; i < 8; i++) my_lba |= ((uint64_t)sector[24 + i]) << (i * 8);
    if (my_lba != 1) {
        result->error_code = 1018;
        snprintf(result->detail, sizeof(result->detail), "MyLBA=%llu (expected 1)", (unsigned long long)my_lba);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint64_t alternate_lba = 0;
    for (int i = 0; i < 8; i++) alternate_lba |= ((uint64_t)sector[32 + i]) << (i * 8);
    if (alternate_lba == 0) {
        result->error_code = 1019;
        strncpy(result->detail, "AlternateLBA=0 (invalid, backup GPT missing)", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t stored_crc = sector[16] | (sector[17] << 8) | (sector[18] << 16) | (sector[19] << 24);
    uint8_t header_copy[92];
    memcpy(header_copy, sector, 92);
    memset(header_copy + 16, 0, 4);
    uint32_t computed_crc = bootdiag_compute_crc32c(header_copy, 92);
    if (computed_crc != stored_crc) {
        result->error_code = 1013;
        snprintf(result->detail, sizeof(result->detail), "Header CRC mismatch: computed=0x%08X, stored=0x%08X", computed_crc, stored_crc);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t num_entries = sector[80] | (sector[81] << 8) | (sector[82] << 16) | (sector[83] << 24);
    uint32_t entry_size = sector[84] | (sector[85] << 8) | (sector[86] << 16) | (sector[87] << 24);
    if (entry_size != 128) {
        result->error_code = 1014;
        snprintf(result->detail, sizeof(result->detail), "PartitionEntrySize=%u (expected 128)", entry_size);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint8_t efi_sp_guid[] = {0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B};
    uint8_t win_guid[]    = {0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};
    uint8_t msr_guid[]    = {0x16,0xE3,0xC9,0xE3,0x5C,0x0B,0x78,0x42,0x81,0x9D,0x9A,0x0C,0x4E,0xFC,0x7E,0x7C};

    int has_efi = 0, has_os = 0, has_msr = 0;

    uint32_t partition_array_sectors = (num_entries * entry_size + 511) / 512;
    uint8_t* part_buf = (uint8_t*)malloc(partition_array_sectors * 512);
    if (!part_buf) {
        result->error_code = 1015;
        strncpy(result->detail, "Out of memory for partition entries", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    for (uint32_t s = 0; s < partition_array_sectors; s++) {
        rc = read_device_sector(ctx, 2 + s, part_buf + s * 512, 512);
        if (rc != 0) {
            free(part_buf);
            result->error_code = 1016;
            snprintf(result->detail, sizeof(result->detail), "Cannot read partition entry LBA %u", 2 + s);
            return BOOTDIAG_RESULT_FAIL;
        }
    }

    uint32_t stored_part_crc = sector[88] | (sector[89] << 8) | (sector[90] << 16) | (sector[91] << 24);
    uint32_t computed_part_crc = bootdiag_compute_crc32c(part_buf, num_entries * entry_size);
    if (computed_part_crc != stored_part_crc) {
        free(part_buf);
        result->error_code = 1020;
        snprintf(result->detail, sizeof(result->detail),
                 "Partition table CRC mismatch: computed=0x%08X, stored=0x%08X", computed_part_crc, stored_part_crc);
        return BOOTDIAG_RESULT_FAIL;
    }

    for (uint32_t i = 0; i < num_entries; i++) {
        uint8_t* entry = part_buf + i * entry_size;
        int all_zero = 1;
        for (int j = 0; j < 16; j++) {
            if (entry[j] != 0) { all_zero = 0; break; }
        }
        if (all_zero) continue;

        partition_count++;
        if (memcmp(entry, efi_sp_guid, 16) == 0) has_efi = 1;
        else if (memcmp(entry, win_guid, 16) == 0) has_os = 1;
        else if (memcmp(entry, msr_guid, 16) == 0) has_msr = 1;
    }
    free(part_buf);

    if (!has_efi || !has_os) {
        result->error_code = 1017;
        snprintf(result->detail, sizeof(result->detail), "Required partition missing: EFI=%d OS=%d MSR=%d", has_efi, has_os, has_msr);
        return BOOTDIAG_RESULT_FAIL;
    }

    snprintf(result->detail, sizeof(result->detail), "%u partitions: EFI=%d OS=%d MSR=%d", partition_count, has_efi, has_os, has_msr);
    return BOOTDIAG_RESULT_PASS;
}

uint32_t bootdiag_v2_bcd(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
    const char* bcd_paths[] = {
        "\\EFI\\Microsoft\\Boot\\BCD",
        "C:\\EFI\\Microsoft\\Boot\\BCD",
        "X:\\EFI\\Microsoft\\Boot\\BCD",
    };

    uint8_t header[4096];
    int found = 0;

    for (int i = 0; i < (int)(sizeof(bcd_paths) / sizeof(bcd_paths[0])); i++) {
        if (read_file_bytes(ctx, bcd_paths[i], 0, header, 4096) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        result->error_code = 2001;
        strncpy(result->detail, "BCD file not found or cannot open", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (memcmp(header, "regf", 4) != 0) {
        result->error_code = 2002;
        snprintf(result->detail, sizeof(result->detail), "BCD hive signature invalid: %.4s (expected 'regf')", header);
        return BOOTDIAG_RESULT_FAIL;
    }

    int has_bootmgr = 0, has_osloader = 0;

    for (uint32_t off = 0; off < 4096 - 4; off++) {
        if (header[off] == '{') {
            if (off + 38 < 4096 && memcmp(header + off, "{9dea8622-5cdd-4e70-acc1-f832d7f0cfb8}", 38) == 0) {
                has_bootmgr = 1;
            }
            if (off + 38 < 4096 && memcmp(header + off, "{466fda14-11a7-4c38-8b39-0eb4ddc42b0c}", 38) == 0) {
                has_osloader = 1;
            }
        }
    }

    if (!has_bootmgr) {
        result->error_code = 2003;
        strncpy(result->detail, "Boot Manager entry {9dea8622} not found in BCD", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (!has_osloader) {
        result->error_code = 2004;
        strncpy(result->detail, "OS Loader entry {466fda14} not found in BCD", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    snprintf(result->detail, sizeof(result->detail), "2 entries: BootMgr=%d OSLoader=%d", has_bootmgr, has_osloader);
    return BOOTDIAG_RESULT_PASS;
}

uint32_t bootdiag_v3_winload(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
    const char* winload_paths[] = {
        "\\Windows\\System32\\winload.efi",
        "C:\\Windows\\System32\\winload.efi",
        "X:\\Windows\\System32\\winload.efi",
    };

    uint8_t header[4096];
    int found = 0;

    for (int i = 0; i < (int)(sizeof(winload_paths) / sizeof(winload_paths[0])); i++) {
        if (read_file_bytes(ctx, winload_paths[i], 0, header, 4096) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        result->error_code = 3001;
        strncpy(result->detail, "winload.efi not found", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (header[0] != 'M' || header[1] != 'Z') {
        result->error_code = 3002;
        snprintf(result->detail, sizeof(result->detail), "MZ signature mismatch: 0x%02X%02X", header[1], header[0]);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t e_lfanew = header[60] | (header[61] << 8) | (header[62] << 16) | (header[63] << 24);
    if (e_lfanew == 0 || e_lfanew >= 4092) {
        result->error_code = 3003;
        snprintf(result->detail, sizeof(result->detail), "e_lfanew=%u invalid", e_lfanew);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (header[e_lfanew] != 'P' || header[e_lfanew + 1] != 'E' || header[e_lfanew + 2] != 0 || header[e_lfanew + 3] != 0) {
        result->error_code = 3004;
        snprintf(result->detail, sizeof(result->detail), "PE signature mismatch at offset %u", e_lfanew);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint16_t machine = header[e_lfanew + 4] | (header[e_lfanew + 5] << 8);
    if (machine != 0x8664) {
        result->error_code = 3005;
        snprintf(result->detail, sizeof(result->detail), "Machine=0x%04X (expected 0x8664 AMD64)", machine);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t pe_offset = e_lfanew + 24;
    if (pe_offset + 2 > 4096) {
        result->error_code = 3006;
        strncpy(result->detail, "PE optional header out of range", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }
    uint16_t magic = header[pe_offset] | (header[pe_offset + 1] << 8);
    if (magic != 0x20B) {
        result->error_code = 3007;
        snprintf(result->detail, sizeof(result->detail), "PE Magic=0x%04X (expected 0x20B PE32+)", magic);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t subsystem_offset = pe_offset + 68;
    if (subsystem_offset + 2 <= 4096) {
        uint16_t subsystem = header[subsystem_offset] | (header[subsystem_offset + 1] << 8);
        if (subsystem != 10 && subsystem != 11) {
            result->error_code = 3008;
            snprintf(result->detail, sizeof(result->detail), "Subsystem=%u (expected 10=EFI_APP or 11=EFI_BOOT)", subsystem);
            return BOOTDIAG_RESULT_FAIL;
        }
        snprintf(result->detail, sizeof(result->detail), "PE64+ EFI, Subsystem=%u", subsystem);
    } else {
        snprintf(result->detail, sizeof(result->detail), "PE64+ EFI (AMD64)");
    }

    return BOOTDIAG_RESULT_PASS;
}

uint32_t bootdiag_v4_block_read(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        result->error_code = 4001;
        strncpy(result->detail, "WSAStartup failed", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }
#endif

    SOCKET sock;
#ifdef _WIN32
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if (sock < 0) {
        result->error_code = 4002;
        strncpy(result->detail, "Socket creation failed", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

#ifdef _WIN32
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->server_port);
    addr.sin_addr.s_addr = inet_addr(ctx->server_addr);

    DWORD timeout_ms = ctx->timeout_sec * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        result->error_code = 4003;
        snprintf(result->detail, sizeof(result->detail), "ImageServer %s:%u unreachable", ctx->server_addr, ctx->server_port);
        return BOOTDIAG_RESULT_FAIL;
    }
#else
    result->error_code = 4099;
    strncpy(result->detail, "Block read verification requires Win32 network API", sizeof(result->detail) - 1);
    close(sock);
    return BOOTDIAG_RESULT_SKIP;
#endif

    uint8_t req_frame[64];
    memset(req_frame, 0, sizeof(req_frame));
    uint32_t magic = 0x48444B47;
    uint16_t version = 1;
    uint8_t opcode = 0x01;
    uint32_t req_id = 1;
    uint32_t payload_len = 24;

    memcpy(req_frame + 0, &magic, 4);
    memcpy(req_frame + 4, &version, 2);
    req_frame[6] = opcode;
    req_frame[7] = 0;
    memcpy(req_frame + 8, &req_id, 4);
    memcpy(req_frame + 12, &payload_len, 4);

    uint64_t test_lbas[] = {0, 1, 2048};
    uint32_t verified = 0;
    uint32_t total_latency_ms = 0;

    for (int i = 0; i < 3; i++) {
        uint64_t lba = test_lbas[i];
        memcpy(req_frame + 16, &lba, 8);
        uint32_t block_count = 1;
        memcpy(req_frame + 24, &block_count, 4);

        uint32_t req_crc = bootdiag_compute_crc32c(req_frame, 60);
        memcpy(req_frame + 60, &req_crc, 4);

#ifdef _WIN32
        uint64_t t1 = GetTickCount64();
        send(sock, (const char*)req_frame, 64, 0);

        uint8_t rsp_frame[64];
        int recv_len = recv(sock, (char*)rsp_frame, 64, 0);
        uint64_t t2 = GetTickCount64();

        if (recv_len > 0) {
            total_latency_ms += (uint32_t)(t2 - t1);
            verified++;
        }
#endif
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#endif

    if (verified < 3) {
        result->error_code = 4004;
        snprintf(result->detail, sizeof(result->detail), "Only %u/3 blocks verified", verified);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t avg_latency = (verified > 0) ? total_latency_ms / verified : 0;
    snprintf(result->detail, sizeof(result->detail), "%u blocks verified, avg latency=%ums", verified, avg_latency);

    if (avg_latency > 1000) {
        size_t cur_len = strlen(result->detail);
        snprintf(result->detail + cur_len, sizeof(result->detail) - cur_len,
                 " (WARN: high latency)");
    }

    return BOOTDIAG_RESULT_PASS;
}

uint32_t bootdiag_v5_paging_io(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
#ifdef _WIN32
    const char* test_file = "\\Windows\\System32\\ntoskrnl.exe";
    const char* alt_paths[] = {
        "C:\\Windows\\System32\\ntoskrnl.exe",
        "X:\\Windows\\System32\\ntoskrnl.exe",
    };

    HANDLE hFile = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 2; i++) {
        hFile = CreateFileA(alt_paths[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) break;
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        result->error_code = 5001;
        strncpy(result->detail, "ntoskrnl.exe not accessible for paging IO test", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint8_t buf[4096];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buf, sizeof(buf), &bytesRead, NULL)) {
        CloseHandle(hFile);
        result->error_code = 5002;
        strncpy(result->detail, "Paging IO read failed", sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    CloseHandle(hFile);
    strncpy(result->detail, "bypasses FastIO, bypasses SmartCache (user-mode verification)", sizeof(result->detail) - 1);
    return BOOTDIAG_RESULT_PASS;
#else
    result->error_code = 5099;
    strncpy(result->detail, "Paging IO path verification requires Win32 API", sizeof(result->detail) - 1);
    return BOOTDIAG_RESULT_SKIP;
#endif
}

uint32_t bootdiag_v6_hive(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
    const char* hive_names[] = {"SYSTEM", "SOFTWARE", "SAM", "SECURITY"};
    const char* hive_paths[][4] = {
        {"\\Windows\\System32\\config\\SYSTEM",    "C:\\Windows\\System32\\config\\SYSTEM",    "X:\\Windows\\System32\\config\\SYSTEM",    NULL},
        {"\\Windows\\System32\\config\\SOFTWARE",  "C:\\Windows\\System32\\config\\SOFTWARE",  "X:\\Windows\\System32\\config\\SOFTWARE",  NULL},
        {"\\Windows\\System32\\config\\SAM",       "C:\\Windows\\System32\\config\\SAM",       "X:\\Windows\\System32\\config\\SAM",       NULL},
        {"\\Windows\\System32\\config\\SECURITY",  "C:\\Windows\\System32\\config\\SECURITY",  "X:\\Windows\\System32\\config\\SECURITY",  NULL},
    };

    uint8_t header[4096];
    int all_pass = 1;
    char detail_buf[256] = {0};
    int offset = 0;

    for (int i = 0; i < 4; i++) {
        int found = 0;
        for (int j = 0; hive_paths[i][j] != NULL; j++) {
            if (read_file_bytes(ctx, hive_paths[i][j], 0, header, 4096) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            all_pass = 0;
            offset += snprintf(detail_buf + offset, sizeof(detail_buf) - offset,
                              "%s:NOT_FOUND ", hive_names[i]);
            continue;
        }

        if (memcmp(header, "regf", 4) != 0) {
            all_pass = 0;
            offset += snprintf(detail_buf + offset, sizeof(detail_buf) - offset,
                              "%s:SIG_INVALID ", hive_names[i]);
            continue;
        }

        uint32_t hive_version = (header[20] | (header[21] << 8) | (header[22] << 16) | (header[23] << 24));
        uint32_t minor_version = (header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24));

        offset += snprintf(detail_buf + offset, sizeof(detail_buf) - offset,
                          "%s:v%u.%u ", hive_names[i], hive_version, minor_version);
    }

    if (!all_pass) {
        result->error_code = 6001;
        strncpy(result->detail, detail_buf, sizeof(result->detail) - 1);
        return BOOTDIAG_RESULT_FAIL;
    }

    strncpy(result->detail, detail_buf, sizeof(result->detail) - 1);
    return BOOTDIAG_RESULT_PASS;
}

uint32_t bootdiag_v7_ntfs(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result) {
    uint8_t sector[512];
    int rc;

    rc = read_device_sector(ctx, 2048, sector, 512);
    if (rc != 0) {
        rc = read_device_sector(ctx, 0, sector, 512);
        if (rc != 0) {
            result->error_code = 7001;
            snprintf(result->detail, sizeof(result->detail), "Cannot read OS partition boot sector: error %d", rc);
            return BOOTDIAG_RESULT_FAIL;
        }
    }

    if (memcmp(sector + 3, "NTFS    ", 8) != 0) {
        result->error_code = 7002;
        char oem[9];
        memcpy(oem, sector + 3, 8);
        oem[8] = '\0';
        snprintf(result->detail, sizeof(result->detail), "NTFS BPB OEM mismatch: '%s'", oem);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint16_t bytes_per_sector = sector[11] | (sector[12] << 8);
    uint8_t sectors_per_cluster = sector[13];
    uint64_t mft_start_lcn = sector[48] | ((uint64_t)sector[49] << 8) | ((uint64_t)sector[50] << 16) | ((uint64_t)sector[51] << 24)
                            | ((uint64_t)sector[52] << 32) | ((uint64_t)sector[53] << 40) | ((uint64_t)sector[54] << 48) | ((uint64_t)sector[55] << 56);

    if (bytes_per_sector != 512 && bytes_per_sector != 4096) {
        result->error_code = 7003;
        snprintf(result->detail, sizeof(result->detail), "BytesPerSector=%u (expected 512 or 4096)", bytes_per_sector);
        return BOOTDIAG_RESULT_FAIL;
    }

    uint32_t cluster_size = bytes_per_sector * (1 << sectors_per_cluster);
    uint64_t mft_lba = 2048 + mft_start_lcn * cluster_size / bytes_per_sector;

    uint8_t mft_record[1024];
    rc = read_device_sector(ctx, mft_lba, mft_record, 512);
    if (rc != 0) {
        result->error_code = 7004;
        snprintf(result->detail, sizeof(result->detail), "$MFT record read failed at LBA %llu", (unsigned long long)mft_lba);
        return BOOTDIAG_RESULT_FAIL;
    }

    if (memcmp(mft_record, "FILE", 4) != 0) {
        result->error_code = 7005;
        snprintf(result->detail, sizeof(result->detail), "$MFT signature mismatch: %.4s", mft_record);
        return BOOTDIAG_RESULT_FAIL;
    }

    snprintf(result->detail, sizeof(result->detail),
             "$MFT:1024B/rec, ClusterSize:%uKB, MFT_LCN:%llu",
             cluster_size / 1024, (unsigned long long)mft_start_lcn);
    return BOOTDIAG_RESULT_PASS;
}
