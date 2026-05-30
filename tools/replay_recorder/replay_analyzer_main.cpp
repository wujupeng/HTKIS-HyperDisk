#include "replay_recorder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace hd::replay;

static void print_usage() {
    printf("boot_replay_analyzer — HTKIS HyperDisk X Replay Analysis Tool\n");
    printf("Usage:\n");
    printf("  boot_replay_analyzer parse <boot_replay.bin>\n");
    printf("  boot_replay_analyzer analyze <boot_replay.bin> [--report=perf]\n");
    printf("  boot_replay_analyzer replay <boot_replay.bin> --target=<server:port>\n");
    printf("  boot_replay_analyzer train <file1.bin> [file2.bin ...] --output=<prefetch_list.txt>\n");
}

static int cmd_parse(const char* path) {
    auto result = BootReplayAnalyzer::Parse(path);
    printf("=== boot_replay.bin Parse Result ===\n");
    printf("Total entries: %u\n", result.entry_count);
    printf("Time range: 0ms ~ %llums\n", (unsigned long long)result.time_range_ms);
    printf("LBA range: 0x%016llX ~ 0x%016llX\n", (unsigned long long)result.lba_min, (unsigned long long)result.lba_max);
    printf("Status distribution: SUCCESS=%u TIMEOUT=%u ERROR=%u RETRY=%u\n",
           result.success_count, result.timeout_count, result.error_count, result.retry_count);
    return 0;
}

static int cmd_analyze(const char* path) {
    auto parse_result = BootReplayAnalyzer::Parse(path);
    if (parse_result.entry_count == 0) {
        printf("No entries found in %s\n", path);
        return 1;
    }

    printf("=== boot_replay.bin Performance Analysis ===\n");
    cmd_parse(path);
    printf("\n");

    auto stages = BootReplayAnalyzer::AnalyzePerf(path);
    for (const auto& stage : stages) {
        printf("Stage: %s (%u IOs)\n", stage.name.c_str(), stage.count);
        printf("  avg=%.1fms  P50=%.1fms  P95=%.1fms  P99=%.1fms\n",
               stage.avg_ms, stage.p50_ms, stage.p95_ms, stage.p99_ms);
    }

    return 0;
}

static int cmd_replay(const char* path, const char* target) {
    std::string server = target;
    uint16_t port = 9527;

    auto colon = server.rfind(':');
    if (colon != std::string::npos) {
        port = static_cast<uint16_t>(std::stoi(server.substr(colon + 1)));
        server = server.substr(0, colon);
    }

    bool ok = BootReplayAnalyzer::Replay(path, server, port);
    return ok ? 0 : 1;
}

static int cmd_train(int file_count, const char** files, const char* output_path) {
    std::vector<std::string> paths;
    for (int i = 0; i < file_count; i++) {
        paths.push_back(files[i]);
    }

    bool ok = BootReplayAnalyzer::Train(paths, output_path);
    if (ok) {
        printf("Training complete. Output: %s\n", output_path);
    } else {
        printf("Training failed.\n");
    }
    return ok ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const char* command = argv[1];

    if (strcmp(command, "parse") == 0) {
        return cmd_parse(argv[2]);
    } else if (strcmp(command, "analyze") == 0) {
        return cmd_analyze(argv[2]);
    } else if (strcmp(command, "replay") == 0) {
        const char* target = "10.10.200.10:9527";
        for (int i = 3; i < argc; i++) {
            if (strncmp(argv[i], "--target=", 9) == 0) {
                target = argv[i] + 9;
            }
        }
        return cmd_replay(argv[2], target);
    } else if (strcmp(command, "train") == 0) {
        const char* output = "prefetch_list.txt";
        int train_start = 2;
        int train_end = argc;

        for (int i = 2; i < argc; i++) {
            if (strncmp(argv[i], "--output=", 9) == 0) {
                output = argv[i] + 9;
                train_end = i;
            }
        }

        int file_count = train_end - train_start;
        if (file_count <= 0) {
            printf("Error: No replay files specified for training\n");
            return 1;
        }

        return cmd_train(file_count, const_cast<const char**>(&argv[train_start]), output);
    } else {
        printf("Unknown command: %s\n", command);
        print_usage();
        return 1;
    }
}
