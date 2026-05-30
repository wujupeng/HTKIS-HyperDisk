#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace hd::overlay {

class RamOverlay {
public:
    bool Write(uint64_t offset, const std::vector<uint8_t>& data);
    bool Read(uint64_t offset, std::vector<uint8_t>& out);
    void Clear();
private:
    std::mutex mutex_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> data_;
};

bool RamOverlay::Write(uint64_t offset, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[offset] = data;
    return true;
}

bool RamOverlay::Read(uint64_t offset, std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(offset);
    if (it != data_.end()) { out = it->second; return true; }
    return false;
}

void RamOverlay::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
}

} // namespace hd::overlay
