#include "hd_log.h"
#include <iostream>

namespace hd::client {

void LogInfo(const std::string& msg)  { std::cout << "[INFO] " << msg << std::endl; }
void LogError(const std::string& msg) { std::cerr << "[ERROR] " << msg << std::endl; }
void LogDebug(const std::string& msg) { std::cout << "[DEBUG] " << msg << std::endl; }

} // namespace hd::client
