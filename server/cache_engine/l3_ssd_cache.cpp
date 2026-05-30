#include "l3_ssd_cache.h"
#include <chrono>

namespace hd::cache {

L3SsdCache::L3SsdCache(uint64_t capacity_bytes)
    : capacity_bytes_(capacity_bytes)
    , used_bytes_(0)
    , hit_count_(0)
    , miss_count_(0)
    , watermark_high_(0.90)
    , watermark_low_(0.80)
{
}

L3SsdCache::~L3SsdCache() = default;

bool L3SsdCache::Lookup(uint64_t block_key, std::vector<uint8_t>& out_data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(block_key);
    if (it != entries_.end()) {
        hit_count_++;
        it->second.access_count++;
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        it->second.last_access_ns = static_cast<uint64_t>(now);

        lru_list_.splice(lru_list_.begin(), lru_list_, lru_map_[block_key]);
        out_data = it->second.data;
        return true;
    }

    miss_count_++;
    return false;
}

void L3SsdCache::Insert(uint64_t block_key, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (entries_.count(block_key)) return;

    auto entry = CacheEntry{
        .block_key = block_key,
        .data = data,
        .access_count = 1,
        .last_access_ns = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
    };

    used_bytes_ += data.size();

    lru_list_.push_front(block_key);
    lru_map_[block_key] = lru_list_.begin();
    entries_[block_key] = std::move(entry);

    EvictIfNeeded();
}

void L3SsdCache::Invalidate(uint64_t block_key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(block_key);
    if (it != entries_.end()) {
        used_bytes_ -= it->second.data.size();
        auto lru_it = lru_map_.find(block_key);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
        entries_.erase(it);
    }
}

void L3SsdCache::EvictIfNeeded()
{
    while (used_bytes_ > static_cast<uint64_t>(capacity_bytes_ * watermark_high_) && !lru_list_.empty()) {
        uint64_t victim = lru_list_.back();
        lru_list_.pop_back();
        lru_map_.erase(victim);

        auto it = entries_.find(victim);
        if (it != entries_.end()) {
            used_bytes_ -= it->second.data.size();
            entries_.erase(it);
        }
    }
}

double L3SsdCache::HitRate() const
{
    uint64_t total = hit_count_ + miss_count_;
    return total > 0 ? static_cast<double>(hit_count_) / static_cast<double>(total) : 0.0;
}

} // namespace hd::cache
