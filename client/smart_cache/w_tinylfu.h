#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cassert>

namespace hd::cache {

struct CacheStats {
    uint64_t hit_count;
    uint64_t miss_count;
    uint64_t eviction_count;
    uint64_t insert_count;
    double   hit_rate;
};

class CountMinSketch {
public:
    CountMinSketch(uint32_t width = 256, uint32_t depth = 4);

    void Add(uint64_t key, uint32_t count = 1);
    uint32_t Estimate(uint64_t key) const;
    void Reset();

private:
    uint32_t width_;
    uint32_t depth_;
    std::vector<std::vector<uint32_t>> table_;
    std::vector<uint32_t> seeds_;

    uint32_t Hash(uint64_t key, uint32_t seed) const;
};

class TinyLFU {
public:
    explicit TinyLFU(uint32_t capacity, uint32_t cm_width = 256, uint32_t cm_depth = 4);

    void Access(uint64_t key);
    uint32_t Estimate(uint64_t key) const;
    void Reset();

private:
    uint32_t capacity_;
    CountMinSketch cm_;
    uint64_t total_access_;
    uint32_t reset_threshold_;
};

struct CacheEntry {
    uint64_t  key;
    void*     value;
    uint32_t  size;
    uint32_t  freq;
    uint64_t  insert_time;
};

class WTinyLFUCache {
public:
    explicit WTinyLFUCache(uint32_t capacity, uint32_t window_ratio = 1,
                           uint32_t protected_ratio = 80);
    ~WTinyLFUCache();

    bool Put(uint64_t key, const void* data, uint32_t size);
    bool Get(uint64_t key, void* buffer, uint32_t buffer_size, uint32_t& out_size);
    bool Contains(uint64_t key) const;
    bool Remove(uint64_t key);
    void Clear();

    uint32_t Count() const;
    uint32_t Capacity() const;
    uint64_t TotalSize() const;
    CacheStats GetStats() const;
    void ResetStats();

private:
    enum class QueueType : uint8_t { Window = 0, Probation = 1, Protected = 2 };

    struct Entry {
        uint64_t  key;
        uint8_t*  data;
        uint32_t  size;
        QueueType queue;
    };

    bool EvictOne();
    void AdaptiveIncrement(uint64_t key);

    uint32_t window_capacity_;
    uint32_t protected_capacity_;
    uint32_t probation_capacity_;
    uint32_t total_capacity_;

    std::unordered_map<uint64_t, Entry> entries_;
    std::vector<uint64_t> window_queue_;
    std::vector<uint64_t> probation_queue_;
    std::vector<uint64_t> protected_queue_;

    TinyLFU tinylfu_;

    mutable std::mutex mutex_;
    std::atomic<uint64_t> hit_count_{0};
    std::atomic<uint64_t> miss_count_{0};
    std::atomic<uint64_t> eviction_count_{0};
    std::atomic<uint64_t> insert_count_{0};
    std::atomic<uint64_t> total_size_{0};
};

} // namespace hd::cache
