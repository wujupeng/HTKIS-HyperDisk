#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace hd::replay {

constexpr uint32_t REPLAY_MAGIC       = 0x48445852;
constexpr uint32_t REPLAY_FOOTER_MAGIC = 0x48445846;
constexpr uint16_t REPLAY_VERSION     = 1;
constexpr uint32_t REPLAY_ENTRY_SIZE  = 33;
constexpr uint32_t REPLAY_HEADER_SIZE = 16;
constexpr uint32_t REPLAY_FOOTER_SIZE = 8;
constexpr uint32_t REPLAY_DEFAULT_CAPACITY = 64 * 1024 * 1024;

enum class ReplayStatus : uint32_t {
    Success = 0,
    Timeout = 1,
    Error   = 2,
    Retry   = 3,
};

#pragma pack(push, 1)

struct ReplayEntry {
    uint64_t lba;
    uint64_t offset;
    uint32_t size;
    uint32_t latency_us;
    uint32_t status;
    uint32_t timestamp_ms;
    uint8_t  reserved;
};

struct ReplayFileHeader {
    uint32_t magic;
    uint16_t version;
    uint32_t entry_count;
    uint32_t header_crc32c;
    uint16_t reserved;
};

struct ReplayFileFooter {
    uint32_t total_crc32c;
    uint32_t footer_magic;
};

#pragma pack(pop)

class ReplayRingBuffer {
public:
    explicit ReplayRingBuffer(uint32_t capacity = REPLAY_DEFAULT_CAPACITY);
    ~ReplayRingBuffer();

    bool Initialize();
    void Destroy();

    void Write(uint64_t lba, uint64_t offset, uint32_t size,
               uint32_t latency_us, uint32_t status, uint8_t reserved = 0);

    void StartRecording();
    void StopRecording();
    bool IsActive() const { return active_.load(std::memory_order_relaxed); }

    uint64_t write_count() const { return write_count_.load(std::memory_order_relaxed); }
    uint64_t overwrite_count() const { return overwrite_count_.load(std::memory_order_relaxed); }
    uint32_t capacity() const { return capacity_; }

    bool FlushToFile(const std::string& path);
    bool FlushToCom1();

    std::vector<ReplayEntry> GetAllEntries() const;

    static uint32_t ComputeCrc32C(const uint8_t* data, uint32_t len);

private:
    uint8_t*   buffer_;
    uint32_t   capacity_;
    std::atomic<uint64_t> write_count_;
    std::atomic<uint64_t> overwrite_count_;
    std::atomic<uint32_t> head_;
    std::atomic<bool>     active_;
    std::chrono::steady_clock::time_point record_start_;
};

class BootReplayRecorder {
public:
    static BootReplayRecorder& Instance();

    bool Start();
    void Stop();

    void RecordIo(uint64_t lba, uint64_t offset, uint32_t size,
                  uint32_t latency_us, uint32_t status, uint8_t reserved = 0);

    bool FlushNormal(const std::string& path = "C:\\HyperDisk\\replay\\boot_replay.bin");
    bool FlushEmergency();
    void CheckAndRenameIncomplete(const std::string& path);

private:
    BootReplayRecorder();
    ReplayRingBuffer ring_;
    std::mutex flush_mutex_;
    bool flushed_;
};

class BootReplayAnalyzer {
public:
    struct ParseResult {
        uint32_t entry_count;
        uint32_t overwrite_count;
        uint64_t time_range_ms;
        uint64_t lba_min;
        uint64_t lba_max;
        uint32_t success_count;
        uint32_t timeout_count;
        uint32_t error_count;
        uint32_t retry_count;
    };

    struct PerfStage {
        std::string name;
        double avg_ms;
        double p50_ms;
        double p95_ms;
        double p99_ms;
        uint32_t count;
    };

    static ParseResult Parse(const std::string& path);
    static std::vector<PerfStage> AnalyzePerf(const std::string& path);
    static bool Replay(const std::string& path, const std::string& target_server, uint16_t port);
    static bool Train(const std::vector<std::string>& paths, const std::string& output_path);

private:
    static std::vector<ReplayEntry> ReadEntries(const std::string& path, ReplayFileHeader& header);
};

} // namespace hd::replay
