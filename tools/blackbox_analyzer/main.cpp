#include "blackbox_types.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>

namespace hd::diag {

static const char* PhaseName(BbPhase phase) {
    switch (phase) {
        case BbPhase::PXE:       return "PXE";
        case BbPhase::IPXE:      return "IPXE";
        case BbPhase::BOOTAGENT: return "BOOTAGENT";
        case BbPhase::WINPE:     return "WINPE";
        case BbPhase::BOOTDIAG:  return "BOOTDIAG";
        case BbPhase::BOOTMGR:   return "BOOTMGR";
        case BbPhase::WINLOAD:   return "WINLOAD";
        case BbPhase::KERNEL:    return "KERNEL";
        case BbPhase::DESKTOP:   return "DESKTOP";
        default:                 return "UNKNOWN";
    }
}

static const char* StatusName(BbStatus status) {
    switch (status) {
        case BbStatus::SUCCESS: return "OK";
        case BbStatus::FAIL:    return "FAIL";
        case BbStatus::TIMEOUT: return "TIMEOUT";
        case BbStatus::RETRY:   return "RETRY";
        case BbStatus::WARN:    return "WARN";
        default:                return "?";
    }
}

int AnalyzeBlackBox(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path);
        return 1;
    }

    BlackBoxFileHeader header{};
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fprintf(stderr, "Cannot read header\n");
        fclose(f);
        return 1;
    }

    if (memcmp(header.magic, BB_MAGIC, 8) != 0) {
        fprintf(stderr, "Invalid magic: %.8s (expected HDBBOX)\n", header.magic);
        fclose(f);
        return 1;
    }

    printf("=== Boot Black Box Analysis ===\n");
    printf("Version:      %u\n", header.version);
    printf("Entry size:   %u bytes\n", header.entry_size);
    printf("Entry count:  %u\n", header.entry_count);
    uint64_t start_time = (uint64_t)header.start_time_lo | ((uint64_t)header.start_time_hi << 32);
    uint64_t end_time = (uint64_t)header.end_time_lo | ((uint64_t)header.end_time_hi << 32);
    printf("Start time:   %llu us\n", (unsigned long long)start_time);
    printf("End time:     %llu us\n", (unsigned long long)end_time);
    printf("Duration:     %.3f seconds\n", (double)(end_time - start_time) / 1000000.0);
    printf("\n");

    std::vector<BlackBoxEntry> entries(header.entry_count);
    if (fread(entries.data(), sizeof(BlackBoxEntry), header.entry_count, f) != header.entry_count) {
        fprintf(stderr, "Failed to read all entries\n");
        fclose(f);
        return 1;
    }

    BlackBoxFileFooter footer{};
    if (fread(&footer, sizeof(footer), 1, f) != 1) {
        fprintf(stderr, "Cannot read footer\n");
        fclose(f);
        return 1;
    }
    fclose(f);

    uint32_t verify_crc = ComputeCrc32C(reinterpret_cast<const uint8_t*>(entries.data()),
                                         header.entry_count * sizeof(BlackBoxEntry));
    printf("CRC32C:       %s (computed=0x%08X, stored=0x%08X)\n",
           verify_crc == footer.total_crc32c ? "OK" : "MISMATCH",
           verify_crc, footer.total_crc32c);
    printf("\n");

    printf("=== IO Timeline ===\n");
    for (uint32_t i = 0; i < entries.size(); i++) {
        const auto& e = entries[i];
        uint64_t ts = (uint64_t)e.timestamp_delta_lo | ((uint64_t)e.timestamp_delta_hi << 32);
        uint64_t lba = (uint64_t)e.lba_lo | ((uint64_t)e.lba_hi << 32);
        uint32_t sz = (uint32_t)e.size_lo | ((uint32_t)e.size_hi << 16);
        printf("[%6u] %10llu us | %-10s | %-7s | LBA=%-8llu off=%-6u sz=%-6u lat=%uus\n",
               i, (unsigned long long)ts,
               PhaseName(static_cast<BbPhase>(e.phase)),
               StatusName(static_cast<BbStatus>(e.status)),
               (unsigned long long)lba, e.offset, sz, e.latency_us);
    }
    printf("\n");

    printf("=== Phase Statistics ===\n");
    std::map<uint8_t, std::vector<uint16_t>> phase_latencies;
    std::map<uint8_t, uint32_t> phase_counts;
    std::map<uint64_t, uint32_t> lba_freq;

    for (const auto& e : entries) {
        phase_latencies[e.phase].push_back(e.latency_us);
        phase_counts[e.phase]++;
        uint64_t lba = (uint64_t)e.lba_lo | ((uint64_t)e.lba_hi << 32);
        lba_freq[lba]++;
    }

    for (const auto& [phase, count] : phase_counts) {
        const auto& lats = phase_latencies[phase];
        double avg = 0;
        for (auto l : lats) avg += l;
        avg /= lats.size();
        std::vector<uint16_t> sorted_lats = lats;
        std::sort(sorted_lats.begin(), sorted_lats.end());
        uint16_t p95 = sorted_lats[sorted_lats.size() * 95 / 100];
        uint16_t p99 = sorted_lats[sorted_lats.size() * 99 / 100];
        printf("  %-10s: %u IOs, avg=%.1fus, P95=%uus, P99=%uus\n",
               PhaseName(static_cast<BbPhase>(phase)), count, avg, p95, p99);
    }
    printf("\n");

    printf("=== Hot Blocks Top-20 ===\n");
    std::vector<std::pair<uint64_t, uint32_t>> hot_blocks(lba_freq.begin(), lba_freq.end());
    std::sort(hot_blocks.begin(), hot_blocks.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < 20 && i < hot_blocks.size(); i++) {
        printf("  LBA=%-12llu count=%u\n", (unsigned long long)hot_blocks[i].first, hot_blocks[i].second);
    }
    printf("\n");

    printf("=== Fault Localization ===\n");
    const BlackBoxEntry* last_success = nullptr;
    const BlackBoxEntry* first_fail = nullptr;
    const BlackBoxEntry* last_meta = nullptr;
    const BlackBoxEntry* last_overlay = nullptr;

    for (const auto& e : entries) {
        if (e.status == static_cast<uint8_t>(BbStatus::SUCCESS)) {
            last_success = &e;
        }
        if (e.status == static_cast<uint8_t>(BbStatus::FAIL) && !first_fail) {
            first_fail = &e;
        }
    }

    if (last_success) {
        uint64_t ts = (uint64_t)last_success->timestamp_delta_lo | ((uint64_t)last_success->timestamp_delta_hi << 32);
        uint64_t lba = (uint64_t)last_success->lba_lo | ((uint64_t)last_success->lba_hi << 32);
        printf("  Last success IO: ts=%llu us, LBA=%llu, phase=%s\n",
               (unsigned long long)ts,
               (unsigned long long)lba,
               PhaseName(static_cast<BbPhase>(last_success->phase)));
    }
    if (first_fail) {
        uint64_t ts = (uint64_t)first_fail->timestamp_delta_lo | ((uint64_t)first_fail->timestamp_delta_hi << 32);
        uint64_t lba = (uint64_t)first_fail->lba_lo | ((uint64_t)first_fail->lba_hi << 32);
        printf("  First failure IO: ts=%llu us, LBA=%llu, phase=%s\n",
               (unsigned long long)ts,
               (unsigned long long)lba,
               PhaseName(static_cast<BbPhase>(first_fail->phase)));
    }

    printf("\nAnalysis complete.\n");
    return 0;
}

} // namespace hd::diag

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: boot_blackbox_analyzer <boot_blackbox.bin>\n");
        return 1;
    }
    return hd::diag::AnalyzeBlackBox(argv[1]);
}
