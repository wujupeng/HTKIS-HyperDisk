#pragma once

#include <cstdint>
#include <cstring>

namespace hd::diag {

constexpr char     BB_MAGIC[8]       = {'H','D','B','B','O','X','\0','\0'};
constexpr uint16_t BB_VERSION        = 1;
constexpr uint32_t BB_ENTRY_SIZE     = 26;
constexpr size_t   BB_RING_SIZE      = 64 * 1024 * 1024;

enum class BbPhase : uint8_t {
    PXE         = 0,
    IPXE        = 1,
    BOOTAGENT   = 2,
    WINPE       = 3,
    BOOTDIAG    = 4,
    BOOTMGR     = 5,
    WINLOAD     = 6,
    KERNEL      = 7,
    DESKTOP     = 8,
};

enum class BbStatus : uint8_t {
    SUCCESS     = 0,
    FAIL        = 1,
    TIMEOUT     = 2,
    RETRY       = 3,
    WARN        = 4,
};

#pragma pack(push, 1)

struct BlackBoxEntry {
    uint32_t timestamp_delta_lo;
    uint32_t timestamp_delta_hi;
    uint8_t  phase;
    uint8_t  status;
    uint16_t latency_us;
    uint32_t lba_lo;
    uint32_t lba_hi;
    uint16_t offset;
    uint16_t size_lo;
    uint8_t  size_hi;
    uint8_t  reserved;
};

struct BlackBoxFileHeader {
    char     magic[8];
    uint16_t version;
    uint16_t entry_size;
    uint32_t entry_count;
    uint32_t start_time_lo;
    uint32_t start_time_hi;
    uint32_t end_time_lo;
    uint32_t end_time_hi;
    uint32_t reserved;
};

struct BlackBoxFileFooter {
    uint32_t total_crc32c;
    uint32_t entry_count_verify;
    uint16_t reserved;
};

#pragma pack(pop)

static_assert(sizeof(BlackBoxEntry) == 26, "BlackBoxEntry must be 26 bytes");
static_assert(sizeof(BlackBoxFileHeader) == 36, "BlackBoxFileHeader must be 36 bytes");
static_assert(sizeof(BlackBoxFileFooter) == 8 + 2, "BlackBoxFileFooter check");

class BlackBoxRecorder {
public:
    static BlackBoxRecorder& Instance();

    bool Initialize();
    void Record(BbPhase phase, uint64_t lba, uint32_t offset, uint32_t size, BbStatus status, uint16_t latency_us);
    bool FlushToDisk(const char* path);
    bool FlushEmergency(const char* path);
    void Shutdown();

    uint32_t overflow_count() const { return overflow_count_; }
    uint32_t entry_count() const { return entry_count_; }

private:
    BlackBoxRecorder() = default;
    uint8_t* ring_buffer_ = nullptr;
    volatile uint32_t write_pos_ = 0;
    volatile uint32_t read_pos_ = 0;
    uint32_t entry_count_ = 0;
    uint32_t overflow_count_ = 0;
    uint64_t start_time_ = 0;
    bool initialized_ = false;
};

uint32_t ComputeCrc32C(const uint8_t* data, uint32_t len);

} // namespace hd::diag
