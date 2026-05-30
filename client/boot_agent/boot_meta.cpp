#include "boot_meta.h"
#include <cstring>
#include <fstream>

namespace hd::boot {

static uint32_t Crc32c(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0x82F63B78 : 0);
    }
    return crc ^ 0xFFFFFFFF;
}

BootMetaCache& BootMetaCache::Instance() {
    static BootMetaCache instance;
    return instance;
}

uint32_t BootMetaCache::ComputeCrc() const {
    size_t crc_offset = offsetof(BootMeta, crc32c);
    return Crc32c(reinterpret_cast<const uint8_t*>(&meta_), crc_offset);
}

bool BootMetaCache::ValidateCrc() {
    uint32_t expected = ComputeCrc();
    return meta_.crc32c == expected;
}

void BootMetaCache::ApplyDefaults() {
    std::memset(&meta_, 0, sizeof(BootMeta));
    meta_.magic = BOOT_META_MAGIC;
    meta_.version = BOOT_META_VERSION;
    meta_.block_size = 4096;
    meta_.write_policy = 0;
    std::strncpy(meta_.primary_server, "10.10.200.10", sizeof(meta_.primary_server) - 1);
    meta_.primary_port = 9527;
}

bool BootMetaCache::Load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        ApplyDefaults();
        valid_ = false;
        return false;
    }

    f.read(reinterpret_cast<char*>(&meta_), sizeof(BootMeta));
    if (!f || meta_.magic != BOOT_META_MAGIC) {
        ApplyDefaults();
        valid_ = false;
        return false;
    }

    if (!ValidateCrc()) {
        ApplyDefaults();
        valid_ = false;
        return false;
    }

    valid_ = true;
    return true;
}

bool BootMetaCache::Save(const std::string& path) {
    meta_.magic = BOOT_META_MAGIC;
    meta_.version = BOOT_META_VERSION;
    meta_.crc32c = ComputeCrc();

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    f.write(reinterpret_cast<const char*>(&meta_), sizeof(BootMeta));
    return f.good();
}

bool BootMetaCache::LoadFromServer(const std::string& server, uint16_t port) {
    (void)server;
    (void)port;
    return false;
}

std::string BootMetaCache::PrimaryServer() const {
    return std::string(meta_.primary_server, strnlen(meta_.primary_server, sizeof(meta_.primary_server)));
}

std::string BootMetaCache::SecondaryServer() const {
    return std::string(meta_.secondary_server, strnlen(meta_.secondary_server, sizeof(meta_.secondary_server)));
}

} // namespace hd::boot
