#include "blackbox_types.h"
#include <cstdio>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace hd::diag {

uint32_t ComputeCrc32C(const uint8_t* data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0x82F63B78 : 0);
    }
    return crc ^ 0xFFFFFFFF;
}

static void FillHeader(BlackBoxFileHeader& header, uint32_t entry_count, uint64_t start_time) {
    memcpy(header.magic, BB_MAGIC, 8);
    header.version = BB_VERSION;
    header.entry_size = BB_ENTRY_SIZE;
    header.entry_count = entry_count;
    header.start_time_lo = (uint32_t)(start_time & 0xFFFFFFFF);
    header.start_time_hi = (uint32_t)(start_time >> 32);
    uint64_t end_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    header.end_time_lo = (uint32_t)(end_time & 0xFFFFFFFF);
    header.end_time_hi = (uint32_t)(end_time >> 32);
    header.reserved = 0;
}

BlackBoxRecorder& BlackBoxRecorder::Instance() {
    static BlackBoxRecorder instance;
    return instance;
}

bool BlackBoxRecorder::Initialize() {
    if (initialized_) return true;

#ifdef _WIN32
    ring_buffer_ = (uint8_t*)VirtualAlloc(NULL, BB_RING_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    ring_buffer_ = (uint8_t*)malloc(BB_RING_SIZE);
#endif
    if (!ring_buffer_) return false;

    memset(ring_buffer_, 0, BB_RING_SIZE);
    write_pos_ = 0;
    read_pos_ = 0;
    entry_count_ = 0;
    overflow_count_ = 0;
    start_time_ = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    initialized_ = true;
    return true;
}

void BlackBoxRecorder::Record(BbPhase phase, uint64_t lba, uint32_t offset, uint32_t size, BbStatus status, uint16_t latency_us) {
    if (!initialized_) return;

    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t delta = now - start_time_;

    BlackBoxEntry entry{};
    entry.timestamp_delta_lo = (uint32_t)(delta & 0xFFFFFFFF);
    entry.timestamp_delta_hi = (uint32_t)(delta >> 32);
    entry.phase = static_cast<uint8_t>(phase);
    entry.status = static_cast<uint8_t>(status);
    entry.latency_us = latency_us;
    entry.lba_lo = (uint32_t)(lba & 0xFFFFFFFF);
    entry.lba_hi = (uint32_t)(lba >> 32);
    entry.offset = (uint16_t)(offset & 0xFFFF);
    entry.size_lo = (uint16_t)(size & 0xFFFF);
    entry.size_hi = (uint8_t)(size >> 16);
    entry.reserved = 0;

    uint32_t pos = write_pos_;
    uint32_t next_pos = pos + sizeof(BlackBoxEntry);

    if (next_pos + sizeof(BlackBoxEntry) > BB_RING_SIZE) {
        overflow_count_++;
        return;
    }

    memcpy(ring_buffer_ + pos, &entry, sizeof(BlackBoxEntry));
    write_pos_ = next_pos;
    entry_count_++;
}

bool BlackBoxRecorder::FlushToDisk(const char* path) {
    if (!initialized_ || entry_count_ == 0) return false;

    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f) return false;

    BlackBoxFileHeader header{};
    FillHeader(header, entry_count_, start_time_);

    fwrite(&header, sizeof(header), 1, f);
    fwrite(ring_buffer_, 1, write_pos_, f);

    uint32_t total_crc = ComputeCrc32C(ring_buffer_, write_pos_);

    BlackBoxFileFooter footer{};
    footer.total_crc32c = total_crc;
    footer.entry_count_verify = entry_count_;
    footer.reserved = 0;
    fwrite(&footer, sizeof(footer), 1, f);

    fclose(f);
    return true;
}

bool BlackBoxRecorder::FlushEmergency(const char* path) {
    return FlushToDisk(path);
}

void BlackBoxRecorder::Shutdown() {
    if (!initialized_) return;

    if (ring_buffer_) {
#ifdef _WIN32
        VirtualFree(ring_buffer_, 0, MEM_RELEASE);
#else
        free(ring_buffer_);
#endif
        ring_buffer_ = nullptr;
    }
    initialized_ = false;
}

} // namespace hd::diag
