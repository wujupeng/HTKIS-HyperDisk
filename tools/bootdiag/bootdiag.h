#pragma once

#include <stdint.h>

#define BOOTDIAG_VERSION_MAJOR 1
#define BOOTDIAG_VERSION_MINOR 0
#define BOOTDIAG_CHECK_COUNT  7

#define BOOTDIAG_RESULT_PASS    0
#define BOOTDIAG_RESULT_FAIL    1
#define BOOTDIAG_RESULT_SKIP    2
#define BOOTDIAG_RESULT_TIMEOUT 3

#define BOOTDIAG_OUTPUT_COM1    0
#define BOOTDIAG_OUTPUT_CONSOLE 1
#define BOOTDIAG_OUTPUT_FILE    2

#define BOOTDIAG_DEFAULT_TIMEOUT_SEC  30
#define BOOTDIAG_DEFAULT_SERVER_PORT  9527

#pragma pack(push, 1)

typedef struct _BOOTDIAG_RESULT {
    uint8_t   check_id;
    char      check_name[32];
    uint8_t   result;
    uint32_t  elapsed_ms;
    char      detail[256];
    uint32_t  error_code;
} BOOTDIAG_RESULT;

typedef struct _BOOTDIAG_CONTEXT {
    char      device_path[256];
    char      server_addr[64];
    uint16_t  server_port;
    uint32_t  timeout_sec;
    uint8_t   output_target;
    uint8_t   verbose;
    char      log_path[260];
    BOOTDIAG_RESULT results[BOOTDIAG_CHECK_COUNT];
    uint32_t  total_pass;
    uint32_t  total_fail;
    uint32_t  total_skip;
    uint64_t  total_elapsed_ms;
    uint8_t   bootdiag_executed;
} BOOTDIAG_CONTEXT;

#pragma pack(pop)

typedef uint32_t (*bootdiag_check_fn)(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);

void bootdiag_output(const BOOTDIAG_CONTEXT* ctx, const char* fmt, ...);
void bootdiag_output_result(const BOOTDIAG_CONTEXT* ctx, const BOOTDIAG_RESULT* r);
uint32_t bootdiag_compute_crc32c(const uint8_t* data, uint32_t len);

uint32_t bootdiag_v1_gpt(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);
uint32_t bootdiag_v2_bcd(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);
uint32_t bootdiag_v3_winload(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);
uint32_t bootdiag_v4_block_read(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);
uint32_t bootdiag_v5_paging_io(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);
uint32_t bootdiag_v6_hive(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);
uint32_t bootdiag_v7_ntfs(BOOTDIAG_CONTEXT* ctx, BOOTDIAG_RESULT* result);

int bootdiag_gate_check(const BOOTDIAG_CONTEXT* ctx);
void bootdiag_update_boot_meta_flags(const BOOTDIAG_CONTEXT* ctx);
