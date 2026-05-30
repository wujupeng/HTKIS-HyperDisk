#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <list>

namespace hd::cache {

struct CacheEntry {
    uint64_t block_key;
    std::vector<uint8_t> data;
    uint32_t access_count;
    uint64_t last_access_ns;
};

class L3SsdCache {
public:
    explicit L3SsdCache(uint64_t capacity_bytes);
    ~L3SsdCache();

    bool Lookup(uint64_t block_key, std::vector<uint8_t>& out_data);
    void Insert(uint64_t block_key, const std::vector<uint8_t>& data);
    void Invalidate(uint64_t block_key);

    double HitRate() const;
    uint64_t UsedBytes() const { return used_bytes_; }
    uint64_t CapacityBytes() const { return capacity_bytes_; }

private:
    void EvictIfNeeded();

    uint64_t capacity_bytes_;
    uint64_t used_bytes_;
    uint64_t hit_count_;
    uint64_t miss_count_;
    double watermark_high_;
    double watermark_low_;

    std::mutex mutex_;
    std::unordered_map<uint64_t, CacheEntry> entries_;
    std::list<uint64_t> lru_list_;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> lru_map_;
};

} // namespace hd::cache
