#include "w_tinylfu.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace hd::cache {

CountMinSketch::CountMinSketch(uint32_t width, uint32_t depth)
    : width_(width)
    , depth_(depth)
    , table_(depth, std::vector<uint32_t>(width, 0))
    , seeds_(depth) {
    for (uint32_t i = 0; i < depth_; i++) {
        seeds_[i] = 2654435761U * (i + 1);
    }
}

uint32_t CountMinSketch::Hash(uint64_t key, uint32_t seed) const {
    uint64_t h = key ^ seed;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return static_cast<uint32_t>(h % width_);
}

void CountMinSketch::Add(uint64_t key, uint32_t count) {
    for (uint32_t d = 0; d < depth_; d++) {
        uint32_t idx = Hash(key, seeds_[d]);
        table_[d][idx] += count;
    }
}

uint32_t CountMinSketch::Estimate(uint64_t key) const {
    uint32_t min_val = UINT32_MAX;
    for (uint32_t d = 0; d < depth_; d++) {
        uint32_t idx = Hash(key, seeds_[d]);
        min_val = std::min(min_val, table_[d][idx]);
    }
    return min_val;
}

void CountMinSketch::Reset() {
    for (auto& row : table_) {
        std::fill(row.begin(), row.end(), 0);
    }
}

TinyLFU::TinyLFU(uint32_t capacity, uint32_t cm_width, uint32_t cm_depth)
    : capacity_(capacity)
    , cm_(cm_width, cm_depth)
    , total_access_(0)
    , reset_threshold_(capacity * 10) {
}

void TinyLFU::Access(uint64_t key) {
    cm_.Add(key, 1);
    total_access_++;
    if (total_access_ >= reset_threshold_) {
        cm_.Reset();
        total_access_ = 0;
    }
}

uint32_t TinyLFU::Estimate(uint64_t key) const {
    return cm_.Estimate(key);
}

void TinyLFU::Reset() {
    cm_.Reset();
    total_access_ = 0;
}

WTinyLFUCache::WTinyLFUCache(uint32_t capacity, uint32_t window_ratio, uint32_t protected_ratio)
    : total_capacity_(capacity)
    , tinylfu_(capacity) {
    uint32_t window_pct = window_ratio;
    uint32_t protected_pct = protected_ratio;
    uint32_t probation_pct = 100 - window_pct - protected_pct;

    window_capacity_ = (capacity * window_pct) / 100;
    protected_capacity_ = (capacity * protected_pct) / 100;
    probation_capacity_ = capacity - window_capacity_ - protected_capacity_;

    if (window_capacity_ == 0) window_capacity_ = 1;
    if (probation_capacity_ == 0) probation_capacity_ = 1;
    if (protected_capacity_ == 0) protected_capacity_ = 1;

    window_queue_.reserve(window_capacity_);
    probation_queue_.reserve(probation_capacity_);
    protected_queue_.reserve(protected_capacity_);
}

WTinyLFUCache::~WTinyLFUCache() {
    Clear();
}

bool WTinyLFUCache::Put(uint64_t key, const void* data, uint32_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(key);
    if (it != entries_.end()) {
        tinylfu_.Access(key);
        if (it->second.data && size > 0) {
            memcpy(it->second.data, data, std::min(size, it->second.size));
        }
        return true;
    }

    while (entries_.size() >= total_capacity_) {
        if (!EvictOne()) break;
    }

    Entry entry{};
    entry.key = key;
    entry.size = size;
    entry.queue = QueueType::Window;
    if (size > 0 && data) {
        entry.data = static_cast<uint8_t*>(malloc(size));
        if (entry.data) {
            memcpy(entry.data, data, size);
        }
    }

    entries_[key] = entry;
    window_queue_.push_back(key);
    insert_count_.fetch_add(1, std::memory_order_relaxed);
    total_size_.fetch_add(size, std::memory_order_relaxed);
    tinylfu_.Access(key);

    return true;
}

bool WTinyLFUCache::Get(uint64_t key, void* buffer, uint32_t buffer_size, uint32_t& out_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end()) {
        miss_count_.fetch_add(1, std::memory_order_relaxed);
        out_size = 0;
        return false;
    }

    tinylfu_.Access(key);
    hit_count_.fetch_add(1, std::memory_order_relaxed);

    const auto& entry = it->second;
    out_size = entry.size;
    if (buffer && entry.data && buffer_size >= entry.size) {
        memcpy(buffer, entry.data, entry.size);
    }

    if (entry.queue == QueueType::Probation) {
        auto pos = std::find(probation_queue_.begin(), probation_queue_.end(), key);
        if (pos != probation_queue_.end()) {
            probation_queue_.erase(pos);
        }
        it->second.queue = QueueType::Protected;
        while (protected_queue_.size() >= protected_capacity_) {
            uint64_t demote_key = protected_queue_.front();
            protected_queue_.erase(protected_queue_.begin());
            auto dit = entries_.find(demote_key);
            if (dit != entries_.end()) {
                dit->second.queue = QueueType::Probation;
                probation_queue_.push_back(demote_key);
            }
        }
        protected_queue_.push_back(key);
    }

    return true;
}

bool WTinyLFUCache::Contains(uint64_t key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.find(key) != entries_.end();
}

bool WTinyLFUCache::Remove(uint64_t key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(key);
    if (it == entries_.end()) return false;

    auto& entry = it->second;
    total_size_.fetch_sub(entry.size, std::memory_order_relaxed);

    switch (entry.queue) {
        case QueueType::Window: {
            auto pos = std::find(window_queue_.begin(), window_queue_.end(), key);
            if (pos != window_queue_.end()) window_queue_.erase(pos);
            break;
        }
        case QueueType::Probation: {
            auto pos = std::find(probation_queue_.begin(), probation_queue_.end(), key);
            if (pos != probation_queue_.end()) probation_queue_.erase(pos);
            break;
        }
        case QueueType::Protected: {
            auto pos = std::find(protected_queue_.begin(), protected_queue_.end(), key);
            if (pos != protected_queue_.end()) protected_queue_.erase(pos);
            break;
        }
    }

    if (entry.data) free(entry.data);
    entries_.erase(it);
    return true;
}

void WTinyLFUCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [k, entry] : entries_) {
        if (entry.data) free(entry.data);
    }
    entries_.clear();
    window_queue_.clear();
    probation_queue_.clear();
    protected_queue_.clear();
    total_size_.store(0, std::memory_order_relaxed);
}

uint32_t WTinyLFUCache::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(entries_.size());
}

uint32_t WTinyLFUCache::Capacity() const {
    return total_capacity_;
}

uint64_t WTinyLFUCache::TotalSize() const {
    return total_size_.load(std::memory_order_relaxed);
}

CacheStats WTinyLFUCache::GetStats() const {
    CacheStats stats{};
    stats.hit_count = hit_count_.load(std::memory_order_relaxed);
    stats.miss_count = miss_count_.load(std::memory_order_relaxed);
    stats.eviction_count = eviction_count_.load(std::memory_order_relaxed);
    stats.insert_count = insert_count_.load(std::memory_order_relaxed);
    uint64_t total = stats.hit_count + stats.miss_count;
    stats.hit_rate = (total > 0) ? static_cast<double>(stats.hit_count) / total : 0.0;
    return stats;
}

void WTinyLFUCache::ResetStats() {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
    eviction_count_.store(0, std::memory_order_relaxed);
    insert_count_.store(0, std::memory_order_relaxed);
}

bool WTinyLFUCache::EvictOne() {
    uint64_t victim_key = 0;
    bool found = false;

    if (!window_queue_.empty()) {
        victim_key = window_queue_.front();
        window_queue_.erase(window_queue_.begin());
        found = true;
    } else if (!probation_queue_.empty()) {
        uint32_t min_freq = UINT32_MAX;
        size_t victim_idx = 0;
        for (size_t i = 0; i < probation_queue_.size() && i < 16; i++) {
            uint64_t k = probation_queue_[i];
            uint32_t f = tinylfu_.Estimate(k);
            if (f < min_freq) {
                min_freq = f;
                victim_key = k;
                victim_idx = i;
                found = true;
                if (min_freq == 0) break;
            }
        }
        if (found) {
            probation_queue_.erase(probation_queue_.begin() + victim_idx);
        }
    } else if (!protected_queue_.empty()) {
        victim_key = protected_queue_.front();
        protected_queue_.erase(protected_queue_.begin());
        found = true;
    }

    if (!found) return false;

    auto it = entries_.find(victim_key);
    if (it != entries_.end()) {
        total_size_.fetch_sub(it->second.size, std::memory_order_relaxed);
        if (it->second.data) free(it->second.data);
        entries_.erase(it);
        eviction_count_.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

} // namespace hd::cache
