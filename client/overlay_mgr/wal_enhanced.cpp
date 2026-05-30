#include "wal_enhanced.h"
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace hd::wal {

static const uint32_t CRC32C_TABLE[256] = {0};
static bool crc_table_init = false;

static void InitCrc32cTable() {
    if (crc_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0x82F63B78 : 0);
        }
        const_cast<uint32_t*>(CRC32C_TABLE)[i] = crc;
    }
    crc_table_init = true;
}

uint32_t ComputeCrc32c(const uint8_t* data, size_t len) {
    InitCrc32cTable();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = CRC32C_TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

std::vector<uint8_t> WalEntryBuilder::Build(uint64_t seq_id, const uint8_t* payload, uint32_t payload_len) {
    size_t total = WAL_HEADER_SIZE + WAL_SEQID_SIZE + WAL_CRC_SIZE +
                   WAL_PAYLOADLEN_SIZE + payload_len + WAL_CRC_SIZE;
    size_t aligned = (total + 7) & ~(size_t)7;
    std::vector<uint8_t> buf(aligned, 0);
    size_t off = 0;

    buf[off++] = (WAL_MAGIC >> 0) & 0xFF;
    buf[off++] = (WAL_MAGIC >> 8) & 0xFF;
    buf[off++] = (WAL_MAGIC >> 16) & 0xFF;
    buf[off++] = (WAL_MAGIC >> 24) & 0xFF;
    buf[off++] = WAL_VERSION & 0xFF;
    buf[off++] = (WAL_VERSION >> 8) & 0xFF;

    for (int i = 0; i < 8; i++) buf[off++] = (seq_id >> (i * 8)) & 0xFF;

    size_t header_crc_start = 0;
    size_t header_crc_end = off;

    std::vector<uint8_t> header_crc_buf;
    header_crc_buf.insert(header_crc_buf.end(), buf.begin(), buf.begin() + off);
    for (int i = 0; i < 4; i++) header_crc_buf.push_back((payload_len >> (i * 8)) & 0xFF);
    uint32_t header_crc = ComputeCrc32c(header_crc_buf.data(), header_crc_buf.size());
    for (int i = 0; i < 4; i++) buf[off++] = (header_crc >> (i * 8)) & 0xFF;

    for (int i = 0; i < 4; i++) buf[off++] = (payload_len >> (i * 8)) & 0xFF;

    if (payload && payload_len > 0) {
        std::memcpy(buf.data() + off, payload, payload_len);
        off += payload_len;
    }

    uint32_t footer_crc = ComputeCrc32c(buf.data(), off);
    for (int i = 0; i < 4; i++) buf[off++] = (footer_crc >> (i * 8)) & 0xFF;

    return buf;
}

WalEntryCheckResult WalEntryBuilder::Validate(const uint8_t* data, size_t len) {
    WalEntryCheckResult result{WalEntryStatus::OK, 0, ""};

    if (len < WAL_HEADER_SIZE + WAL_SEQID_SIZE + WAL_CRC_SIZE + WAL_PAYLOADLEN_SIZE + WAL_CRC_SIZE) {
        result.status = WalEntryStatus::TORN_WRITE_DETECTED;
        result.error_msg = "Entry too short";
        return result;
    }

    size_t off = 0;
    uint32_t magic = 0;
    for (int i = 0; i < 4; i++) magic |= (uint32_t)data[off++] << (i * 8);
    if (magic != WAL_MAGIC) {
        result.status = WalEntryStatus::TORN_WRITE_DETECTED;
        result.error_msg = "Magic mismatch";
        return result;
    }

    off += 2;

    uint64_t seq_id = 0;
    for (int i = 0; i < 8; i++) seq_id |= (uint64_t)data[off++] << (i * 8);
    result.seq_id = seq_id;

    uint32_t stored_header_crc = 0;
    for (int i = 0; i < 4; i++) stored_header_crc |= (uint32_t)data[off++] << (i * 8);

    uint32_t payload_len = 0;
    for (int i = 0; i < 4; i++) payload_len |= (uint32_t)data[off++] << (i * 8);

    size_t expected_len = WAL_HEADER_SIZE + WAL_SEQID_SIZE + WAL_CRC_SIZE +
                          WAL_PAYLOADLEN_SIZE + payload_len + WAL_CRC_SIZE;
    if (len < expected_len) {
        result.status = WalEntryStatus::TORN_WRITE_DETECTED;
        result.error_msg = "Payload truncated";
        return result;
    }

    std::vector<uint8_t> header_crc_buf;
    header_crc_buf.insert(header_crc_buf.end(), data, data + WAL_HEADER_SIZE + WAL_SEQID_SIZE);
    for (int i = 0; i < 4; i++) header_crc_buf.push_back((payload_len >> (i * 8)) & 0xFF);
    uint32_t computed_header_crc = ComputeCrc32c(header_crc_buf.data(), header_crc_buf.size());
    if (computed_header_crc != stored_header_crc) {
        result.status = WalEntryStatus::TORN_WRITE_DETECTED;
        result.error_msg = "Header CRC mismatch (torn write)";
        return result;
    }

    size_t footer_off = expected_len - WAL_CRC_SIZE;
    uint32_t stored_footer_crc = 0;
    for (int i = 0; i < 4; i++) stored_footer_crc |= (uint32_t)data[footer_off + i] << (i * 8);

    uint32_t computed_footer_crc = ComputeCrc32c(data, footer_off);
    if (computed_footer_crc != stored_footer_crc) {
        result.status = WalEntryStatus::TORN_WRITE_DETECTED;
        result.error_msg = "Footer CRC mismatch (torn write)";
        return result;
    }

    return result;
}

WalEntry WalEntryBuilder::Parse(const uint8_t* data, size_t len) {
    WalEntry entry{};
    size_t off = 0;

    entry.header.magic = 0;
    for (int i = 0; i < 4; i++) entry.header.magic |= (uint32_t)data[off++] << (i * 8);
    entry.header.version = data[off] | (data[off + 1] << 8);
    off += 2;

    entry.seq_id = 0;
    for (int i = 0; i < 8; i++) entry.seq_id |= (uint64_t)data[off++] << (i * 8);

    entry.header_crc = 0;
    for (int i = 0; i < 4; i++) entry.header_crc |= (uint32_t)data[off++] << (i * 8);

    entry.payload_len = 0;
    for (int i = 0; i < 4; i++) entry.payload_len |= (uint32_t)data[off++] << (i * 8);

    if (entry.payload_len > 0) {
        entry.payload.assign(data + off, data + off + entry.payload_len);
        off += entry.payload_len;
    }

    entry.footer_crc = 0;
    for (int i = 0; i < 4; i++) entry.footer_crc |= (uint32_t)data[off + i] << (i * 8);

    return entry;
}

bool SequenceFence::CheckMonotonic(uint64_t seq_id) {
    return seq_id > Current();
}

WalSegment::WalSegment(const std::string& path, uint64_t start_seq_id, bool read_only)
    : path_(path)
    , start_seq_id_(start_seq_id)
    , last_seq_id_(start_seq_id)
    , entry_count_(0)
    , file_size_(0)
    , is_sealed_(read_only)
    , read_only_(read_only)
    , fd_(-1)
{
    if (read_only) {
        fd_ = open(path.c_str(), O_RDONLY);
    } else {
        fd_ = open(path.c_str(), O_RDWR | O_CREAT, 0644);
    }
    if (fd_ >= 0) {
        struct stat st;
        if (fstat(fd_, &st) == 0) {
            file_size_ = static_cast<uint64_t>(st.st_size);
        }
    }
}

WalSegment::~WalSegment() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

bool WalSegment::WriteEntry(uint64_t seq_id, const uint8_t* payload, uint32_t payload_len) {
    if (is_sealed_ || read_only_ || fd_ < 0) return false;

    auto buf = WalEntryBuilder::Build(seq_id, payload, payload_len);

    std::lock_guard<std::mutex> lock(write_mutex_);
    ssize_t n = write(fd_, buf.data(), buf.size());
    if (n != static_cast<ssize_t>(buf.size())) return false;

    fsync(fd_);

    last_seq_id_ = seq_id;
    entry_count_++;
    file_size_ += buf.size();
    return true;
}

std::vector<WalEntry> WalSegment::ReadAllEntries() {
    std::vector<WalEntry> entries;
    if (fd_ < 0) return entries;

    std::vector<uint8_t> data(file_size_);
    lseek(fd_, 0, SEEK_SET);
    ssize_t n = read(fd_, data.data(), file_size_);
    if (n < 0) return entries;

    size_t offset = 0;
    while (offset < static_cast<size_t>(n)) {
        auto result = WalEntryBuilder::Validate(data.data() + offset, n - offset);
        if (result.status != WalEntryStatus::OK) {
            break;
        }

        auto entry = WalEntryBuilder::Parse(data.data() + offset, n - offset);
        size_t entry_size = WAL_HEADER_SIZE + WAL_SEQID_SIZE + WAL_CRC_SIZE +
                            WAL_PAYLOADLEN_SIZE + entry.payload_len + WAL_CRC_SIZE;
        entry_size = (entry_size + 7) & ~(size_t)7;

        entries.push_back(std::move(entry));
        offset += entry_size;
    }

    return entries;
}

bool WalSegment::IsFull() const {
    return file_size_ >= WAL_SEGMENT_SIZE || entry_count_ >= WAL_MAX_ENTRIES;
}

void WalSegment::Seal() {
    if (!is_sealed_) {
        Fsync();
        is_sealed_ = true;
    }
}

void WalSegment::Fsync() {
    if (fd_ >= 0) fsync(fd_);
}

WalManager::WalManager(const std::string& data_dir)
    : data_dir_(data_dir)
    , next_segment_number_(1)
    , is_open_(false)
{
}

WalManager::~WalManager() {
    Close();
}

bool WalManager::Open() {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    std::filesystem::create_directories(data_dir_);

    auto cp = ReadCheckpoint();
    seq_fence_.Reset(cp.flushed_seq_id + 1);

    std::string seg_path = SegmentPath(next_segment_number_);
    active_segment_ = std::make_unique<WalSegment>(seg_path, seq_fence_.Current(), false);
    is_open_ = true;
    return true;
}

void WalManager::Close() {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    if (active_segment_) {
        active_segment_->Seal();
        active_segment_.reset();
    }
    sealed_segments_.clear();
    is_open_ = false;
}

uint64_t WalManager::WriteEntry(const uint8_t* payload, uint32_t payload_len) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    if (!is_open_ || !active_segment_) return 0;

    uint64_t seq_id = seq_fence_.Next();

    if (!active_segment_->WriteEntry(seq_id, payload, payload_len)) {
        return 0;
    }

    RollOverIfNeeded();
    return seq_id;
}

bool WalManager::WriteCheckpoint(uint64_t flushed_seq_id) {
    std::string cp_path = CheckpointPath();
    int fd = open(cp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string line = std::to_string(flushed_seq_id) + "," +
                       std::to_string(now) + "," +
                       (active_segment_ ? active_segment_->Path() : "") + "\n";
    ssize_t n = write(fd, line.c_str(), line.size());
    close(fd);
    return n > 0;
}

Checkpoint WalManager::ReadCheckpoint() {
    Checkpoint cp{0, 0, ""};
    std::string cp_path = CheckpointPath();

    int fd = open(cp_path.c_str(), O_RDONLY);
    if (fd < 0) return cp;

    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return cp;

    buf[n] = '\0';
    char* save = nullptr;
    char* token = strtok_r(buf, ",", &save);
    if (token) cp.flushed_seq_id = strtoull(token, nullptr, 10);
    token = strtok_r(nullptr, ",", &save);
    if (token) cp.timestamp_ms = strtoull(token, nullptr, 10);
    token = strtok_r(nullptr, "\n", &save);
    if (token) cp.segment_path = token;

    return cp;
}

std::vector<WalEntry> WalManager::CollectEntriesForReplay() {
    std::vector<WalEntry> all;

    for (auto& seg : sealed_segments_) {
        auto entries = seg->ReadAllEntries();
        all.insert(all.end(), std::make_move_iterator(entries.begin()),
                   std::make_move_iterator(entries.end()));
    }

    if (active_segment_) {
        auto entries = active_segment_->ReadAllEntries();
        all.insert(all.end(), std::make_move_iterator(entries.begin()),
                   std::make_move_iterator(entries.end()));
    }

    return all;
}

bool WalManager::CrashReplay() {
    auto cp = ReadCheckpoint();
    auto entries = CollectEntriesForReplay();

    uint64_t last_flushed = cp.flushed_seq_id;
    uint64_t prev_seq_id = 0;
    bool segment_corrupted = false;

    for (auto& entry : entries) {
        if (segment_corrupted) break;
        if (entry.seq_id <= last_flushed) continue;
        if (entry.seq_id <= prev_seq_id) {
            prev_seq_id = entry.seq_id;
            continue;
        }
        if (entry.seq_id - prev_seq_id > 1 && prev_seq_id > 0) {
        }
        prev_seq_id = entry.seq_id;
    }

    WriteCheckpoint(prev_seq_id);
    return true;
}

void WalManager::TriggerSegmentRollover() {
    if (active_segment_) {
        active_segment_->Seal();
        sealed_segments_.push_back(std::move(active_segment_));
    }

    next_segment_number_++;
    std::string seg_path = SegmentPath(next_segment_number_);
    active_segment_ = std::make_unique<WalSegment>(seg_path, seq_fence_.Current(), false);
}

bool WalManager::RollOverIfNeeded() {
    if (active_segment_ && active_segment_->IsFull()) {
        TriggerSegmentRollover();
        return true;
    }
    return false;
}

std::string WalManager::SegmentPath(uint32_t seg_num) {
    char buf[64];
    snprintf(buf, sizeof(buf), "wal-seg-%06d.log", seg_num);
    return data_dir_ + "/" + buf;
}

std::string WalManager::CheckpointPath() {
    return data_dir_ + "/wal.ckpt";
}

uint32_t WalManager::CurrentSegmentNumber() {
    return next_segment_number_;
}

} // namespace hd::wal
