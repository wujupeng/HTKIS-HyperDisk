#pragma once
#include <string>

namespace hd::client {

void LogInfo(const std::string& msg);
void LogError(const std::string& msg);
void LogDebug(const std::string& msg);

} // namespace hd::client
