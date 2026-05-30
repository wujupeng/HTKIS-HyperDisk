#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hd::boot {

#pragma pack(push, 1)

struct BootMeta {
    uint32_t magic;
    uint16_t version;
    uint64_t image_id;
    uint64_t disk_size;
    uint32_t block_size;
    uint32_t block_count;
    uint8_t  layer_id;
    uint8_t  write_policy;
    uint8_t  reserved1[2];
    char     primary_server[64];
    uint16_t primary_port;
    char     secondary_server[64];
    uint16_t secondary_port;
    char     overlay_path[128];
    uint64_t block_map_offset;
    uint32_t block_map_count;
    uint32_t crc32c;
};

#pragma pack(pop)

constexpr uint32_t BOOT_META_MAGIC   = 0x4844424D;
constexpr uint16_t BOOT_META_VERSION = 1;
constexpr const char* BOOT_META_PATH = "boot.meta";

class BootMetaCache {
public:
    static BootMetaCache& Instance();

    bool Load(const std::string& path);
    bool Save(const std::string& path);
    bool LoadFromServer(const std::string& server, uint16_t port);

    const BootMeta& Meta() const { return meta_; }
    bool IsValid() const { return valid_; }

    std::string PrimaryServer() const;
    std::string SecondaryServer() const;

private:
    BootMetaCache() = default;
    BootMeta meta_{};
    bool valid_{false};

    bool ValidateCrc();
    void ApplyDefaults();
    uint32_t ComputeCrc() const;
};

} // namespace hd::boot
