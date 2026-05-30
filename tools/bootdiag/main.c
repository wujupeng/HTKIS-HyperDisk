#include "bootdiag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

static uint32_t crc32c_table[256];
static int crc32c_table_init = 0;

static void crc32c_init_table(void) {
    if (crc32c_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78;
            else
                crc >>= 1;
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_init = 1;
}

uint32_t bootdiag_compute_crc32c(const uint8_t* data, uint32_t len) {
    crc32c_init_table();
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32c_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

static uint64_t bootdiag_get_tick_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

void bootdiag_output(const BOOTDIAG_CONTEXT* ctx, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (ctx->output_target == BOOTDIAG_OUTPUT_CONSOLE || ctx->verbose) {
        printf("%s\n", buf);
        fflush(stdout);
    }

    if (ctx->output_target == BOOTDIAG_OUTPUT_FILE && ctx->log_path[0]) {
        FILE* f = fopen(ctx->log_path, "a");
        if (f) {
            fprintf(f, "%s\n", buf);
            fclose(f);
        }
    }
}

void bootdiag_output_result(const BOOTDIAG_CONTEXT* ctx, const BOOTDIAG_RESULT* r) {
    const char* status_str;
    switch (r->result) {
        case BOOTDIAG_RESULT_PASS:    status_str = "PASS"; break;
        case BOOTDIAG_RESULT_FAIL:    status_str = "FAIL"; break;
        case BOOTDIAG_RESULT_SKIP:    status_str = "SKIP"; break;
        case BOOTDIAG_RESULT_TIMEOUT: status_str = "TIMEOUT"; break;
        default:                      status_str = "UNKNOWN"; break;
    }
    bootdiag_output(ctx, "  [%d] %-28s %s   %ums", r->check_id, r->check_name, status_str, r->elapsed_ms);
    if (r->detail[0]) {
        bootdiag_output(ctx, "      %s", r->detail);
    }
}

int bootdiag_gate_check(const BOOTDIAG_CONTEXT* ctx) {
    return (bootdiag_critical_fail_count(ctx) > 0) ? 1 : 0;
}

int bootdiag_is_critical_check(uint8_t check_id) {
    return (check_id >= 1 && check_id <= BOOTDIAG_CHECK_CRITICAL_COUNT) ? 1 : 0;
}

uint32_t bootdiag_critical_fail_count(const BOOTDIAG_CONTEXT* ctx) {
    uint32_t count = 0;
    for (int i = 0; i < BOOTDIAG_CHECK_CRITICAL_COUNT; i++) {
        if (ctx->results[i].result == BOOTDIAG_RESULT_FAIL) {
            count++;
        }
    }
    return count;
}

void bootdiag_update_boot_meta_flags(const BOOTDIAG_CONTEXT* ctx) {
    uint16_t flags = 0;
    if (ctx->bootdiag_executed) flags |= (1 << 2);
    for (int i = 0; i < BOOTDIAG_CHECK_COUNT; i++) {
        if (ctx->results[i].result == BOOTDIAG_RESULT_FAIL) {
            flags |= (1 << (3 + i));
        }
    }
    if (ctx->total_fail == 0 && ctx->total_pass == BOOTDIAG_CHECK_COUNT) {
        flags |= (1 << 10);
    }

    bootdiag_output(ctx, "boot.meta flags update: 0x%04X", flags);
}

static void print_usage(void) {
    printf("bootdiag.exe — HTKIS HyperDisk X Boot Diagnostic Tool v%d.%d\n",
           BOOTDIAG_VERSION_MAJOR, BOOTDIAG_VERSION_MINOR);
    printf("Usage: bootdiag.exe [options]\n");
    printf("Options:\n");
    printf("  --device=<path>     Virtual disk device path\n");
    printf("  --server=<addr>     ImageServer address (default: 10.10.200.10)\n");
    printf("  --port=<port>       ImageServer port (default: 9527)\n");
    printf("  --timeout=<sec>     Per-check timeout in seconds (default: 30)\n");
    printf("  --output=<target>   Output target: com1|console|file (default: console)\n");
    printf("  --log=<path>        Log file path (default: C:\\HyperDisk\\diag\\bootdiag.log)\n");
    printf("  --verbose           Verbose output mode\n");
    printf("  --skip=<checks>     Skip checks (comma-separated: 1,2,3,4,5,6,7)\n");
    printf("  --retry=<count>     Retry count for failed checks (default: 0)\n");
    printf("  --help              Show this help\n");
}

int main(int argc, char* argv[]) {
    BOOTDIAG_CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));

    strncpy(ctx.device_path, "\\\\.\\HyperDisk0", sizeof(ctx.device_path) - 1);
    strncpy(ctx.server_addr, "10.10.200.10", sizeof(ctx.server_addr) - 1);
    ctx.server_port = BOOTDIAG_DEFAULT_SERVER_PORT;
    ctx.timeout_sec = BOOTDIAG_DEFAULT_TIMEOUT_SEC;
    ctx.output_target = BOOTDIAG_OUTPUT_CONSOLE;
    strncpy(ctx.log_path, "C:\\HyperDisk\\diag\\bootdiag.log", sizeof(ctx.log_path) - 1);

    uint8_t skip_checks[BOOTDIAG_CHECK_COUNT] = {0};
    uint32_t retry_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--device=", 9) == 0) {
            strncpy(ctx.device_path, argv[i] + 9, sizeof(ctx.device_path) - 1);
        } else if (strncmp(argv[i], "--server=", 9) == 0) {
            strncpy(ctx.server_addr, argv[i] + 9, sizeof(ctx.server_addr) - 1);
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            ctx.server_port = (uint16_t)atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--timeout=", 10) == 0) {
            ctx.timeout_sec = (uint32_t)atoi(argv[i] + 10);
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            const char* t = argv[i] + 9;
            if (strcmp(t, "com1") == 0) ctx.output_target = BOOTDIAG_OUTPUT_COM1;
            else if (strcmp(t, "console") == 0) ctx.output_target = BOOTDIAG_OUTPUT_CONSOLE;
            else if (strcmp(t, "file") == 0) ctx.output_target = BOOTDIAG_OUTPUT_FILE;
        } else if (strncmp(argv[i], "--log=", 6) == 0) {
            strncpy(ctx.log_path, argv[i] + 6, sizeof(ctx.log_path) - 1);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            ctx.verbose = 1;
        } else if (strncmp(argv[i], "--skip=", 7) == 0) {
            char skip_buf[64];
            strncpy(skip_buf, argv[i] + 7, sizeof(skip_buf) - 1);
            skip_buf[sizeof(skip_buf) - 1] = '\0';
            char* tok = strtok(skip_buf, ",");
            while (tok) {
                int id = atoi(tok);
                if (id >= 1 && id <= BOOTDIAG_CHECK_COUNT) skip_checks[id - 1] = 1;
                tok = strtok(NULL, ",");
            }
        } else if (strncmp(argv[i], "--retry=", 8) == 0) {
            retry_count = (uint32_t)atoi(argv[i] + 8);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
    }

    bootdiag_check_fn checks[BOOTDIAG_CHECK_COUNT] = {
        bootdiag_v1_gpt,
        bootdiag_v2_bcd,
        bootdiag_v3_winload,
        bootdiag_v4_block_read,
        bootdiag_v5_paging_io,
        bootdiag_v6_hive,
        bootdiag_v7_ntfs,
    };

    const char* check_names[BOOTDIAG_CHECK_COUNT] = {
        "GPT Partition Table",
        "BCD Boot Configuration",
        "winload.efi Readable",
        "Block Read (ImageServer)",
        "Paging IO Path",
        "Registry Hive",
        "NTFS Metadata",
    };

    bootdiag_output(&ctx, "========================================");
    bootdiag_output(&ctx, "bootdiag.exe v%d.%d — HTKIS HyperDisk X",
                    BOOTDIAG_VERSION_MAJOR, BOOTDIAG_VERSION_MINOR);
    bootdiag_output(&ctx, "Device: %s", ctx.device_path);
    bootdiag_output(&ctx, "Server: %s:%u", ctx.server_addr, ctx.server_port);
    bootdiag_output(&ctx, "Timeout: %us per check", ctx.timeout_sec);
    bootdiag_output(&ctx, "========================================");

    uint64_t total_start = bootdiag_get_tick_ms();

    for (int i = 0; i < BOOTDIAG_CHECK_COUNT; i++) {
        if (skip_checks[i]) {
            ctx.results[i].check_id = (uint8_t)(i + 1);
            strncpy(ctx.results[i].check_name, check_names[i], sizeof(ctx.results[i].check_name) - 1);
            ctx.results[i].result = BOOTDIAG_RESULT_SKIP;
            ctx.results[i].elapsed_ms = 0;
            strncpy(ctx.results[i].detail, "Skipped by --skip option", sizeof(ctx.results[i].detail) - 1);
            ctx.total_skip++;
            bootdiag_output_result(&ctx, &ctx.results[i]);
            continue;
        }

        ctx.results[i].check_id = (uint8_t)(i + 1);
        strncpy(ctx.results[i].check_name, check_names[i], sizeof(ctx.results[i].check_name) - 1);

        uint32_t check_result;
        uint32_t attempt = 0;

        do {
            uint64_t check_start = bootdiag_get_tick_ms();
            check_result = checks[i](&ctx, &ctx.results[i]);
            uint64_t check_end = bootdiag_get_tick_ms();
            ctx.results[i].elapsed_ms = (uint32_t)(check_end - check_start);
            ctx.results[i].result = (uint8_t)check_result;
            attempt++;
        } while (check_result != BOOTDIAG_RESULT_PASS && attempt <= retry_count);

        if (check_result == BOOTDIAG_RESULT_PASS) {
            ctx.total_pass++;
        } else {
            ctx.total_fail++;
        }

        bootdiag_output_result(&ctx, &ctx.results[i]);
    }

    uint64_t total_end = bootdiag_get_tick_ms();
    ctx.total_elapsed_ms = total_end - total_start;

    bootdiag_output(&ctx, "============================================");
    bootdiag_output(&ctx, "Total: %u/%u PASSED, %u FAILED, %u SKIPPED, Time: %llums",
                    ctx.total_pass, BOOTDIAG_CHECK_COUNT, ctx.total_fail, ctx.total_skip,
                    (unsigned long long)ctx.total_elapsed_ms);

    if (bootdiag_gate_check(&ctx) == 0) {
        if (ctx.total_fail == 0) {
            bootdiag_output(&ctx, "ALL CHECKS PASSED — safe to proceed to BootMgr");
        } else {
            bootdiag_output(&ctx, "CRITICAL CHECKS PASSED — non-critical failures, degraded boot allowed");
            for (int i = BOOTDIAG_CHECK_CRITICAL_COUNT; i < BOOTDIAG_CHECK_COUNT; i++) {
                if (ctx.results[i].result == BOOTDIAG_RESULT_FAIL) {
                    bootdiag_output(&ctx, "  WARN V%d (%s): %s", i + 1, ctx.results[i].check_name, ctx.results[i].detail);
                }
            }
        }
        ctx.bootdiag_executed = 1;
    } else {
        bootdiag_output(&ctx, "CRITICAL CHECKS FAILED — blocking BootMgr startup");
        bootdiag_output(&ctx, "Critical failures:");
        for (int i = 0; i < BOOTDIAG_CHECK_CRITICAL_COUNT; i++) {
            if (ctx.results[i].result == BOOTDIAG_RESULT_FAIL) {
                bootdiag_output(&ctx, "  FAIL V%d (%s): %s", i + 1, ctx.results[i].check_name, ctx.results[i].detail);
            }
        }
        ctx.bootdiag_executed = 1;
    }

    bootdiag_update_boot_meta_flags(&ctx);

    return bootdiag_gate_check(&ctx);
}
