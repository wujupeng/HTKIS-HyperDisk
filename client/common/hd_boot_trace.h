#pragma once

#include <cstdint>
#include <atomic>

namespace hd::trace {

constexpr uint32_t BOOT_TRACE_BUF_SIZE = 65536;
constexpr uint8_t  BOOT_TRACE_EVENT_SIZE = 64;

#pragma pack(push, 1)

enum class BootIoType : uint8_t {
    Read       = 0,
    Write      = 1,
    CacheHit   = 2,
    CacheMiss  = 3,
    NetSend    = 4,
    NetRecv    = 5,
    OverlayHit = 6,
    Prefetch   = 7,
};

struct BootIoEvent {
    uint64_t   timestamp_ns;
    BootIoType io_type;
    uint8_t    layer_id;
    uint8_t    cache_level;
    uint8_t    reserved;
    uint32_t   status;
    uint64_t   block_addr;
    uint32_t   block_count;
    uint32_t   latency_us;
    uint64_t   image_id;
    uint64_t   request_id;
    uint8_t    padding[16];
};

#pragma pack(pop)

class BootIoTrace {
public:
    static BootIoTrace& Instance();

    void RecordEvent(const BootIoEvent& event);
    void RecordIo(BootIoType type, uint64_t block_addr, uint32_t block_count,
                  uint32_t latency_us, uint8_t cache_level, uint32_t status);
    uint32_t GetEvents(BootIoEvent* buffer, uint32_t max_count);
    void Reset();
    uint32_t Count() const;

private:
    BootIoTrace();

    BootIoEvent buffer_[BOOT_TRACE_BUF_SIZE];
    std::atomic<uint32_t> write_pos_;
    std::atomic<uint32_t> count_;
    std::atomic<uint64_t> next_request_id_;
};

} // namespace hd::trace
