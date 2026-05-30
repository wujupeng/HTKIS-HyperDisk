#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace hd::image {

struct BlockRequest {
    uint64_t image_id;
    uint64_t block_offset;
    uint32_t block_count;
    uint8_t  layer_id;
};

struct BlockResponse {
    uint64_t image_id;
    uint64_t block_offset;
    uint32_t block_count;
    int32_t  status;
    std::vector<uint8_t> data;
};

struct ImageFile {
    uint64_t image_id;
    std::string name;
    std::string path;
    uint64_t total_size;
    uint32_t block_count;
    int      fd;
};

class ImageService {
public:
    ImageService(const std::string& data_dir);
    ~ImageService();

    bool CreateImage(uint64_t image_id, const std::string& name, uint64_t size);
    bool DeleteImage(uint64_t image_id);
    BlockResponse ReadBlocks(const BlockRequest& req);
    bool WriteBlocks(const BlockRequest& req, const std::vector<uint8_t>& data);
    std::vector<ImageFile> ListImages();

private:
    std::string data_dir_;
    std::mutex images_mutex_;
    std::unordered_map<uint64_t, ImageFile> images_;

    ImageFile* FindImage(uint64_t image_id);
};

} // namespace hd::image
