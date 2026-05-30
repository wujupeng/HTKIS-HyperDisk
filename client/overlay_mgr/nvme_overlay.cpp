#include <cstdint>
#include <vector>
namespace hd::overlay {
class NvmeOverlay { public: bool Write(uint64_t, const std::vector<uint8_t>&) { return true; } };
} // namespace hd::overlay
