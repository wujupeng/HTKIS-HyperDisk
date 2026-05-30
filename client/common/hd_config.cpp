#include "hd_config.h"
#include <unordered_map>

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (P)
#endif

namespace hd::client {

Config& Config::Instance() {
    static Config instance;
    return instance;
}

bool Config::Load(const std::string& path) {
    UNREFERENCED_PARAMETER(path);
    return true;
}

std::string Config::GetString(const std::string& key, const std::string& default_val) {
    UNREFERENCED_PARAMETER(key);
    return default_val;
}

int64_t Config::GetInt(const std::string& key, int64_t default_val) {
    UNREFERENCED_PARAMETER(key);
    return default_val;
}

} // namespace hd::client
