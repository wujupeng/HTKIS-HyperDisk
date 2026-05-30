#include "hd_boot_trace.h"
#include <chrono>
#include <cstring>

namespace hd::trace {

BootIoTrace::BootIoTrace()
    : write_pos_(0), count_(0), next_request_id_(1)
{
    std::memset(buffer_, 0, sizeof(buffer_));
}

BootIoTrace& BootIoTrace::Instance() {
    static BootIoTrace instance;
    return instance;
}

void BootIoTrace::RecordEvent(const BootIoEvent& event) {
    uint32_t pos = write_pos_.fetch_add(1, std::memory_order_relaxed) % BOOT_TRACE_BUF_SIZE;
    buffer_[pos] = event;
    count_.fetch_add(1, std::memory_order_relaxed);
}

void BootIoTrace::RecordIo(BootIoType type, uint64_t block_addr, uint32_t block_count,
                            uint32_t latency_us, uint8_t cache_level, uint32_t status) {
    BootIoEvent event{};
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    event.timestamp_ns = static_cast<uint64_t>(now.count());
    event.io_type = type;
    event.block_addr = block_addr;
    event.block_count = block_count;
    event.latency_us = latency_us;
    event.cache_level = cache_level;
    event.status = status;
    event.request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
    RecordEvent(event);
}

uint32_t BootIoTrace::GetEvents(BootIoEvent* out_buffer, uint32_t max_count) {
    uint32_t total = count_.load(std::memory_order_relaxed);
    uint32_t to_copy = (total < max_count) ? total : max_count;
    uint32_t pos = write_pos_.load(std::memory_order_relaxed);

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t src_idx = (pos - to_copy + i) % BOOT_TRACE_BUF_SIZE;
        out_buffer[i] = buffer_[src_idx];
    }

    return to_copy;
}

void BootIoTrace::Reset() {
    write_pos_.store(0, std::memory_order_relaxed);
    count_.store(0, std::memory_order_relaxed);
}

uint32_t BootIoTrace::Count() const {
    return count_.load(std::memory_order_relaxed);
}

} // namespace hd::trace
