#include "hd_log.h"
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (P)
#endif
namespace hd::client {
void IpcSend(const std::string& port_name, const void* data, size_t len) {
    UNREFERENCED_PARAMETER(port_name); UNREFERENCED_PARAMETER(data); UNREFERENCED_PARAMETER(len);
}
} // namespace hd::client
