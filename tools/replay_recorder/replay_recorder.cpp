#include "replay_recorder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace hd::replay {

static uint32_t crc32c_table[256];
static bool crc32c_initialized = false;

static void EnsureCrc32cInit() {
    if (crc32c_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0x82F63B78) : (crc >> 1);
        }
        crc32c_table[i] = crc;
    }
    crc32c_initialized = true;
}

uint32_t ReplayRingBuffer::ComputeCrc32C(const uint8_t* data, uint32_t len) {
    EnsureCrc32cInit();
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32c_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

ReplayRingBuffer::ReplayRingBuffer(uint32_t capacity)
    : buffer_(nullptr)
    , capacity_(capacity)
    , write_count_(0)
    , overwrite_count_(0)
    , head_(0)
    , active_(false) {
}

ReplayRingBuffer::~ReplayRingBuffer() {
    Destroy();
}

bool ReplayRingBuffer::Initialize() {
    if (buffer_) return true;
#ifdef _WIN32
    buffer_ = (uint8_t*)VirtualAlloc(NULL, capacity_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    buffer_ = (uint8_t*)aligned_alloc(4096, capacity_);
#endif
    if (!buffer_) return false;
    memset(buffer_, 0, capacity_);
    return true;
}

void ReplayRingBuffer::Destroy() {
    if (buffer_) {
#ifdef _WIN32
        VirtualFree(buffer_, 0, MEM_RELEASE);
#else
        free(buffer_);
#endif
        buffer_ = nullptr;
    }
    active_.store(false, std::memory_order_relaxed);
}

void ReplayRingBuffer::Write(uint64_t lba, uint64_t offset, uint32_t size,
                              uint32_t latency_us, uint32_t status, uint8_t reserved) {
    if (!active_.load(std::memory_order_relaxed)) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - record_start_);
    uint32_t timestamp_ms = static_cast<uint32_t>(elapsed.count());

    ReplayEntry entry{};
    entry.lba = lba;
    entry.offset = offset;
    entry.size = size;
    entry.latency_us = latency_us;
    entry.status = status;
    entry.timestamp_ms = timestamp_ms;
    entry.reserved = reserved;

    uint32_t write_pos = head_.load(std::memory_order_relaxed);
    if (write_pos + REPLAY_ENTRY_SIZE > capacity_) {
        write_pos = 0;
    }
    memcpy(buffer_ + write_pos, &entry, REPLAY_ENTRY_SIZE);
    head_.store((write_pos + REPLAY_ENTRY_SIZE) % capacity_, std::memory_order_relaxed);
    write_count_.fetch_add(1, std::memory_order_relaxed);

    uint64_t wc = write_count_.load(std::memory_order_relaxed);
    uint64_t max_entries = capacity_ / REPLAY_ENTRY_SIZE;
    if (wc > max_entries) {
        overwrite_count_.store(wc - max_entries, std::memory_order_relaxed);
    }
}

void ReplayRingBuffer::StartRecording() {
    record_start_ = std::chrono::steady_clock::now();
    active_.store(true, std::memory_order_relaxed);
}

void ReplayRingBuffer::StopRecording() {
    active_.store(false, std::memory_order_relaxed);
}

std::vector<ReplayEntry> ReplayRingBuffer::GetAllEntries() const {
    std::vector<ReplayEntry> entries;
    if (!buffer_) return entries;

    uint64_t wc = write_count_.load(std::memory_order_relaxed);
    uint64_t max_entries = capacity_ / REPLAY_ENTRY_SIZE;
    uint32_t count = static_cast<uint32_t>(std::min(wc, max_entries));

    uint32_t read_start;
    uint64_t oc = overwrite_count_.load(std::memory_order_relaxed);
    if (oc > 0) {
        read_start = head_.load(std::memory_order_relaxed);
    } else {
        read_start = 0;
    }

    entries.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pos = (read_start + i * REPLAY_ENTRY_SIZE) % capacity_;
        ReplayEntry entry;
        memcpy(&entry, buffer_ + pos, REPLAY_ENTRY_SIZE);
        entries.push_back(entry);
    }

    return entries;
}

bool ReplayRingBuffer::FlushToFile(const std::string& path) {
    StopRecording();
    auto entries = GetAllEntries();
    if (entries.empty()) return false;

    ReplayFileHeader header{};
    header.magic = REPLAY_MAGIC;
    header.version = REPLAY_VERSION;
    header.entry_count = static_cast<uint32_t>(entries.size());
    header.reserved = 0;

    uint8_t header_buf[REPLAY_HEADER_SIZE];
    memset(header_buf, 0, REPLAY_HEADER_SIZE);
    memcpy(header_buf, &header, 12);
    header.header_crc32c = ComputeCrc32C(header_buf, 12);
    memcpy(header_buf + 12, &header.header_crc32c, 4);
    memcpy(header_buf + 16, &header.reserved, 2);

    std::vector<uint8_t> file_buf;
    file_buf.reserve(REPLAY_HEADER_SIZE + entries.size() * REPLAY_ENTRY_SIZE + REPLAY_FOOTER_SIZE);
    file_buf.insert(file_buf.end(), header_buf, header_buf + REPLAY_HEADER_SIZE);

    for (const auto& e : entries) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&e);
        file_buf.insert(file_buf.end(), p, p + REPLAY_ENTRY_SIZE);
    }

    ReplayFileFooter footer{};
    footer.total_crc32c = ComputeCrc32C(file_buf.data(), static_cast<uint32_t>(file_buf.size()));
    footer.footer_magic = REPLAY_FOOTER_MAGIC;
    const uint8_t* fp = reinterpret_cast<const uint8_t*>(&footer);
    file_buf.insert(file_buf.end(), fp, fp + REPLAY_FOOTER_SIZE);

#ifdef _WIN32
    std::string dir = path;
    auto last_sep = dir.find_last_of("\\/");
    if (last_sep != std::string::npos) {
        dir = dir.substr(0, last_sep);
        std::string mkdir_cmd = "if not exist \"" + dir + "\" mkdir \"" + dir + "\"";
        system(mkdir_cmd.c_str());
    }
#endif

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(file_buf.data()), file_buf.size());
    ofs.flush();
    ofs.close();

    return true;
}

bool ReplayRingBuffer::FlushToCom1() {
    StopRecording();
    auto entries = GetAllEntries();

#ifdef _WIN32
    HANDLE hCom = CreateFileA("COM1", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hCom == INVALID_HANDLE_VALUE) return false;

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(hCom, &dcb);
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(hCom, &dcb);

    auto write_com1 = [&](const char* msg) {
        DWORD written = 0;
        WriteFile(hCom, msg, static_cast<DWORD>(strlen(msg)), &written, NULL);
    };

    char buf[256];
    snprintf(buf, sizeof(buf), "Replay: EMERGENCY — %llu entries\r\n",
             (unsigned long long)entries.size());
    write_com1(buf);

    uint32_t count = std::min(static_cast<uint32_t>(entries.size()), 1000u);
    for (uint32_t i = 0; i < count; i++) {
        const auto& e = entries[i];
        snprintf(buf, sizeof(buf), "[%u] LBA=%llu S=%u L=%uus T=%ums\r\n",
                 i, (unsigned long long)e.lba, e.status, e.latency_us, e.timestamp_ms);
        write_com1(buf);
    }

    write_com1("Replay: EMERGENCY dump complete\r\n");
    CloseHandle(hCom);
    return true;
#else
    return false;
#endif
}

BootReplayRecorder::BootReplayRecorder()
    : ring_(REPLAY_DEFAULT_CAPACITY)
    , flushed_(false) {
}

BootReplayRecorder& BootReplayRecorder::Instance() {
    static BootReplayRecorder instance;
    return instance;
}

bool BootReplayRecorder::Start() {
    if (!ring_.Initialize()) return false;
    ring_.StartRecording();
    flushed_ = false;
    return true;
}

void BootReplayRecorder::Stop() {
    ring_.StopRecording();
}

void BootReplayRecorder::RecordIo(uint64_t lba, uint64_t offset, uint32_t size,
                                   uint32_t latency_us, uint32_t status, uint8_t reserved) {
    ring_.Write(lba, offset, size, latency_us, status, reserved);
}

bool BootReplayRecorder::FlushNormal(const std::string& path) {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    if (flushed_) return true;
    bool ok = ring_.FlushToFile(path);
    if (ok) flushed_ = true;
    return ok;
}

bool BootReplayRecorder::FlushEmergency() {
    std::lock_guard<std::mutex> lock(flush_mutex_);
    if (flushed_) return true;

    const std::string path = "C:\\HyperDisk\\replay\\boot_replay.bin";
    if (ring_.FlushToFile(path)) {
        flushed_ = true;
        return true;
    }

    return ring_.FlushToCom1();
}

void BootReplayRecorder::CheckAndRenameIncomplete(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return;

    ifs.seekg(0, std::ios::end);
    auto file_size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (file_size < REPLAY_HEADER_SIZE + REPLAY_FOOTER_SIZE) {
        ifs.close();

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif
        char suffix[32];
        strftime(suffix, sizeof(suffix), "_%Y%m%d_%H%M%S", &tm_buf);

        std::string new_path = path;
        auto dot = new_path.rfind('.');
        if (dot != std::string::npos) {
            new_path = new_path.substr(0, dot) + std::string("_incomplete") + suffix + new_path.substr(dot);
        } else {
            new_path = new_path + std::string("_incomplete") + suffix;
        }

#ifdef _WIN32
        MoveFileA(path.c_str(), new_path.c_str());
#else
        rename(path.c_str(), new_path.c_str());
#endif
    }
}

std::vector<ReplayEntry> BootReplayAnalyzer::ReadEntries(const std::string& path, ReplayFileHeader& header) {
    std::vector<ReplayEntry> entries;
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return entries;

    uint8_t header_buf[REPLAY_HEADER_SIZE];
    ifs.read(reinterpret_cast<char*>(header_buf), REPLAY_HEADER_SIZE);
    if (!ifs) return entries;

    memcpy(&header, header_buf, sizeof(ReplayFileHeader));
    if (header.magic != REPLAY_MAGIC || header.version != REPLAY_VERSION) return entries;

    entries.resize(header.entry_count);
    for (uint32_t i = 0; i < header.entry_count; i++) {
        uint8_t entry_buf[REPLAY_ENTRY_SIZE];
        ifs.read(reinterpret_cast<char*>(entry_buf), REPLAY_ENTRY_SIZE);
        if (!ifs) {
            entries.resize(i);
            break;
        }
        memcpy(&entries[i], entry_buf, REPLAY_ENTRY_SIZE);
    }

    return entries;
}

BootReplayAnalyzer::ParseResult BootReplayAnalyzer::Parse(const std::string& path) {
    ParseResult result{};
    ReplayFileHeader header;
    auto entries = ReadEntries(path, header);
    if (entries.empty()) return result;

    result.entry_count = static_cast<uint32_t>(entries.size());
    result.lba_min = UINT64_MAX;
    result.lba_max = 0;
    uint32_t max_ts = 0;

    for (const auto& e : entries) {
        result.lba_min = std::min(result.lba_min, e.lba);
        result.lba_max = std::max(result.lba_max, e.lba);
        max_ts = std::max(max_ts, e.timestamp_ms);

        switch (static_cast<ReplayStatus>(e.status)) {
            case ReplayStatus::Success: result.success_count++; break;
            case ReplayStatus::Timeout: result.timeout_count++; break;
            case ReplayStatus::Error:   result.error_count++; break;
            case ReplayStatus::Retry:   result.retry_count++; break;
            default: break;
        }
    }
    result.time_range_ms = max_ts;

    return result;
}

std::vector<BootReplayAnalyzer::PerfStage> BootReplayAnalyzer::AnalyzePerf(const std::string& path) {
    std::vector<PerfStage> stages;
    ReplayFileHeader header;
    auto entries = ReadEntries(path, header);
    if (entries.empty()) return stages;

    struct StageRange { const char* name; uint32_t start_ms; uint32_t end_ms; };
    StageRange ranges[] = {
        {"BootStart",    0,     10000},
        {"DriverLoad",   10000, 15000},
        {"DesktopInit",  15000, 30000},
    };

    for (const auto& range : ranges) {
        std::vector<double> latencies;
        for (const auto& e : entries) {
            if (e.timestamp_ms >= range.start_ms && e.timestamp_ms < range.end_ms) {
                latencies.push_back(static_cast<double>(e.latency_us) / 1000.0);
            }
        }

        if (latencies.empty()) continue;

        PerfStage stage;
        stage.name = range.name;
        stage.count = static_cast<uint32_t>(latencies.size());

        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        stage.avg_ms = sum / latencies.size();

        std::sort(latencies.begin(), latencies.end());
        stage.p50_ms = latencies[latencies.size() * 50 / 100];
        stage.p95_ms = latencies[latencies.size() * 95 / 100];
        stage.p99_ms = latencies[latencies.size() * 99 / 100];

        stages.push_back(stage);
    }

    return stages;
}

bool BootReplayAnalyzer::Replay(const std::string& path, const std::string& target_server, uint16_t port) {
    ReplayFileHeader header;
    auto entries = ReadEntries(path, header);
    if (entries.empty()) return false;

    (void)target_server;
    (void)port;

    printf("Replay: %u entries to %s:%u\n", header.entry_count, target_server.c_str(), port);
    printf("Replay: stub — requires ImageServer TCP connection implementation\n");
    return true;
}

bool BootReplayAnalyzer::Train(const std::vector<std::string>& paths, const std::string& output_path) {
    std::vector<ReplayEntry> all_entries;
    for (const auto& p : paths) {
        ReplayFileHeader header;
        auto entries = ReadEntries(p, header);
        all_entries.insert(all_entries.end(), entries.begin(), entries.end());
    }

    if (all_entries.empty()) return false;

    struct LbaFreq { uint64_t lba; uint32_t freq; };
    std::vector<LbaFreq> freqs;
    for (const auto& e : all_entries) {
        auto it = std::find_if(freqs.begin(), freqs.end(), [&](const LbaFreq& f) { return f.lba == e.lba; });
        if (it != freqs.end()) {
            it->freq++;
        } else {
            freqs.push_back({e.lba, 1});
        }
    }

    std::sort(freqs.begin(), freqs.end(), [](const LbaFreq& a, const LbaFreq& b) { return a.freq > b.freq; });

    std::ofstream ofs(output_path);
    if (!ofs) return false;

    ofs << "# HTKIS HyperDisk X Static Prefetch Hot Block List\n";
    ofs << "# Format: lba,size,priority,phase\n";

    uint32_t top_k = std::min(static_cast<uint32_t>(freqs.size()), 500u);
    for (uint32_t i = 0; i < top_k; i++) {
        const auto& f = freqs[i];
        const char* phase = "bootstart";
        uint32_t priority = top_k - i;

        for (const auto& e : all_entries) {
            if (e.lba == f.lba) {
                if (e.timestamp_ms > 15000) phase = "desktop";
                else if (e.timestamp_ms > 10000) phase = "driverload";
                break;
            }
        }

        ofs << f.lba << ",4096," << priority << "," << phase << "\n";
    }

    return true;
}

} // namespace hd::replay
