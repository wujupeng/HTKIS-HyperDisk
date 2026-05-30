#include "image_service.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace hd::image {

constexpr size_t BLOCK_SIZE = 4096;

ImageService::ImageService(const std::string& data_dir)
    : data_dir_(data_dir)
{
}

ImageService::~ImageService()
{
    for (auto& [id, img] : images_) {
        if (img.fd >= 0) {
            close(img.fd);
        }
    }
}

bool ImageService::CreateImage(uint64_t image_id, const std::string& name, uint64_t size)
{
    std::lock_guard<std::mutex> lock(images_mutex_);

    if (images_.count(image_id)) return false;

    ImageFile img{};
    img.image_id = image_id;
    img.name = name;
    img.path = data_dir_ + "/" + std::to_string(image_id) + ".img";
    img.total_size = size;
    img.block_count = static_cast<uint32_t>(size / BLOCK_SIZE);

    img.fd = open(img.path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (img.fd < 0) return false;

    if (ftruncate(img.fd, static_cast<off_t>(size)) != 0) {
        close(img.fd);
        return false;
    }

    images_[image_id] = std::move(img);
    return true;
}

bool ImageService::DeleteImage(uint64_t image_id)
{
    std::lock_guard<std::mutex> lock(images_mutex_);

    auto it = images_.find(image_id);
    if (it == images_.end()) return false;

    if (it->second.fd >= 0) {
        close(it->second.fd);
    }
    unlink(it->second.path.c_str());
    images_.erase(it);
    return true;
}

BlockResponse ImageService::ReadBlocks(const BlockRequest& req)
{
    BlockResponse resp{};
    resp.image_id = req.image_id;
    resp.block_offset = req.block_offset;
    resp.block_count = req.block_count;

    std::lock_guard<std::mutex> lock(images_mutex_);
    auto* img = FindImage(req.image_id);
    if (!img) {
        resp.status = -1;
        return resp;
    }

    size_t total_bytes = static_cast<size_t>(req.block_count) * BLOCK_SIZE;
    resp.data.resize(total_bytes);

    off_t offset = static_cast<off_t>(req.block_offset * BLOCK_SIZE);
    ssize_t n = pread(img->fd, resp.data.data(), total_bytes, offset);

    if (n < 0) {
        resp.status = -2;
        resp.data.clear();
    } else {
        resp.status = 0;
        resp.data.resize(static_cast<size_t>(n));
    }

    return resp;
}

bool ImageService::WriteBlocks(const BlockRequest& req, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(images_mutex_);
    auto* img = FindImage(req.image_id);
    if (!img) return false;

    off_t offset = static_cast<off_t>(req.block_offset * BLOCK_SIZE);
    ssize_t n = pwrite(img->fd, data.data(), data.size(), offset);
    return n == static_cast<ssize_t>(data.size());
}

std::vector<ImageFile> ImageService::ListImages()
{
    std::lock_guard<std::mutex> lock(images_mutex_);
    std::vector<ImageFile> result;
    for (auto& [id, img] : images_) {
        result.push_back(img);
    }
    return result;
}

ImageFile* ImageService::FindImage(uint64_t image_id)
{
    auto it = images_.find(image_id);
    return it != images_.end() ? &it->second : nullptr;
}

} // namespace hd::image
