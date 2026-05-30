#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace hd::cache {

class L1RamCache {
public:
    explicit L1RamCache(uint64_t capacity_bytes);
    bool Lookup(uint64_t key, std::vector<uint8_t>& out);
    void Insert(uint64_t key, const std::vector<uint8_t>& data);
    void Invalidate(uint64_t key);
    double HitRate() const;
private:
    uint64_t capacity_;
    uint64_t used_;
    uint64_t hits_;
    uint64_t misses_;
    std::mutex mutex_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> store_;
};

L1RamCache::L1RamCache(uint64_t capacity_bytes) : capacity_(capacity_bytes), used_(0), hits_(0), misses_(0) {}
bool L1RamCache::Lookup(uint64_t key, std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = store_.find(key);
    if (it != store_.end()) { hits_++; out = it->second; return true; }
    misses_++; return false;
}
void L1RamCache::Insert(uint64_t key, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!store_.count(key)) used_ += data.size();
    store_[key] = data;
}
void L1RamCache::Invalidate(uint64_t key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = store_.find(key);
    if (it != store_.end()) { used_ -= it->second.size(); store_.erase(it); }
}
double L1RamCache::HitRate() const {
    uint64_t total = hits_ + misses_;
    return total > 0 ? static_cast<double>(hits_) / total : 0.0;
}

} // namespace hd::cache
