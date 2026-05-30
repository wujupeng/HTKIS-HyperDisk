#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <fstream>

namespace hd::wal {

constexpr uint32_t WAL_MAGIC    = 0x48445857;
constexpr uint16_t WAL_VERSION  = 1;
constexpr uint32_t WAL_SEGMENT_SIZE   = 64 * 1024 * 1024;
constexpr uint32_t WAL_MAX_ENTRIES    = 500000;
constexpr uint8_t  WAL_HEADER_SIZE    = 6;
constexpr uint8_t  WAL_SEQID_SIZE     = 8;
constexpr uint8_t  WAL_CRC_SIZE       = 4;
constexpr uint8_t  WAL_PAYLOADLEN_SIZE = 4;

#pragma pack(push, 1)

struct WalEntryHeader {
    uint32_t magic;
    uint16_t version;
};

struct WalEntry {
    WalEntryHeader header;
    uint64_t       seq_id;
    uint32_t       header_crc;
    uint32_t       payload_len;
    std::vector<uint8_t> payload;
    uint32_t       footer_crc;
};

#pragma pack(pop)

enum class WalEntryStatus {
    OK = 0,
    TORN_WRITE_DETECTED = 1,
    SEQ_ID_VIOLATION = 2,
    CRC_MISMATCH = 3,
};

struct WalEntryCheckResult {
    WalEntryStatus status;
    uint64_t       seq_id;
    std::string    error_msg;
};

uint32_t ComputeCrc32c(const uint8_t* data, size_t len);

class WalEntryBuilder {
public:
    static std::vector<uint8_t> Build(uint64_t seq_id, const uint8_t* payload, uint32_t payload_len);
    static WalEntryCheckResult Validate(const uint8_t* data, size_t len);
    static WalEntry Parse(const uint8_t* data, size_t len);
};

class SequenceFence {
public:
    SequenceFence() : next_seq_id_(1) {}
    uint64_t Next() { return next_seq_id_.fetch_add(1, std::memory_order_seq_cst); }
    uint64_t Current() const { return next_seq_id_.load(std::memory_order_seq_cst) - 1; }
    void Reset(uint64_t start) { next_seq_id_.store(start, std::memory_order_seq_cst); }
    bool CheckMonotonic(uint64_t seq_id);

private:
    std::atomic<uint64_t> next_seq_id_;
};

class WalSegment {
public:
    WalSegment(const std::string& path, uint64_t start_seq_id, bool read_only = false);
    ~WalSegment();

    bool WriteEntry(uint64_t seq_id, const uint8_t* payload, uint32_t payload_len);
    std::vector<WalEntry> ReadAllEntries();
    bool IsFull() const;
    bool IsSealed() const { return is_sealed_; }
    void Seal();
    void Fsync();
    uint64_t StartSeqId() const { return start_seq_id_; }
    uint64_t LastSeqId() const { return last_seq_id_; }
    uint32_t EntryCount() const { return entry_count_; }
    uint64_t FileSize() const { return file_size_; }
    const std::string& Path() const { return path_; }

private:
    std::string path_;
    uint64_t start_seq_id_;
    uint64_t last_seq_id_;
    uint32_t entry_count_;
    uint64_t file_size_;
    bool is_sealed_;
    bool read_only_;
    std::mutex write_mutex_;
    int fd_;
};

struct Checkpoint {
    uint64_t flushed_seq_id;
    uint64_t timestamp_ms;
    std::string segment_path;
};

class WalManager {
public:
    explicit WalManager(const std::string& data_dir);
    ~WalManager();

    bool Open();
    void Close();
    uint64_t WriteEntry(const uint8_t* payload, uint32_t payload_len);
    bool WriteCheckpoint(uint64_t flushed_seq_id);
    Checkpoint ReadCheckpoint();
    std::vector<WalEntry> CollectEntriesForReplay();
    bool CrashReplay();
    void TriggerSegmentRollover();

    uint64_t CurrentSeqId() const { return seq_fence_.Current(); }
    const std::string& DataDir() const { return data_dir_; }

private:
    bool RollOverIfNeeded();
    std::string SegmentPath(uint32_t seg_num);
    std::string CheckpointPath();
    uint32_t CurrentSegmentNumber();

    std::string data_dir_;
    SequenceFence seq_fence_;
    std::unique_ptr<WalSegment> active_segment_;
    std::vector<std::unique_ptr<WalSegment>> sealed_segments_;
    std::mutex manager_mutex_;
    uint32_t next_segment_number_;
    bool is_open_;
};

} // namespace hd::wal
