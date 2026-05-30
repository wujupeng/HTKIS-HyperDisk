#pragma once
#include <cstdint>
#include <string>

namespace hd::client {

class Config {
public:
    static Config& Instance();
    bool Load(const std::string& path);
    std::string GetString(const std::string& key, const std::string& default_val);
    int64_t GetInt(const std::string& key, int64_t default_val);
private:
    Config() = default;
};

} // namespace hd::client
