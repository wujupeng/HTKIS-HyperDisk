#include "boot_agent.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>

namespace hd::boot {

static const char* PhaseTag(BootPhase phase) {
    switch (phase) {
        case BootPhase::PXE_START:       return "PXE_START";
        case BootPhase::IPXE_START:      return "IPXE_START";
        case BootPhase::BOOTAGENT_START: return "BOOTAGENT_START";
        case BootPhase::DHCP_DISCOVER:   return "DHCP_DISCOVER";
        case BootPhase::GRPC_REGISTER:   return "GRPC_REGISTER";
        case BootPhase::CHUNKSTREAM_DL:  return "CHUNKSTREAM_DL";
        case BootPhase::INTEGRITY_CHECK: return "INTEGRITY_CHECK";
        case BootPhase::WINPE_START:     return "WINPE_START";
        case BootPhase::BOOTDIAG_RUN:    return "BOOTDIAG_RUN";
        default:                         return "UNKNOWN";
    }
}

static const char* FailTag(BootFailCode code) {
    switch (code) {
        case BootFailCode::NONE:           return "NONE";
        case BootFailCode::PXE_TIMEOUT:    return "PXE_TIMEOUT";
        case BootFailCode::TFTP_FAIL:      return "TFTP_FAIL";
        case BootFailCode::DHCP_FAIL:      return "DHCP_FAIL";
        case BootFailCode::GRPC_FAIL:      return "METACENTER_FAIL";
        case BootFailCode::IMGSRV_FAIL:    return "IMGSRV_FAIL";
        case BootFailCode::CRC_FAIL:       return "BOOTMETA_CRC_FAIL";
        case BootFailCode::SIGNATURE_FAIL: return "WINLOAD_SIG_FAIL";
        case BootFailCode::BOOTMETA_FAIL:  return "BOOTMETA_FAIL";
        default:                           return "UNKNOWN_FAIL";
    }
}

Com1Logger& Com1Logger::Instance() {
    static Com1Logger instance;
    return instance;
}

void Com1Logger::Initialize() {
    if (initialized_) return;

    com1_handle_ = CreateFileA(
        "\\\\.\\COM1",
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (com1_handle_ != INVALID_HANDLE_VALUE) {
        DCB dcb{};
        dcb.DCBlength = sizeof(DCB);
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        SetCommState(com1_handle_, &dcb);
    }

    initialized_ = true;
}

void Com1Logger::WriteRaw(const char* str) {
    if (!initialized_) Initialize();

    if (com1_handle_ != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(com1_handle_, str, static_cast<DWORD>(strlen(str)), &written, nullptr);
    }
}

void Com1Logger::Write(BootPhase phase, const char* fmt, ...) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[HDx:%s] ", PhaseTag(phase));

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);

    size_t len = strlen(buf);
    if (len < sizeof(buf) - 2) {
        buf[len] = '\r';
        buf[len + 1] = '\n';
        buf[len + 2] = '\0';
    }

    WriteRaw(buf);
}

void Com1Logger::WriteFail(BootFailCode code, const char* fmt, ...) {
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[HDx:%s] ", FailTag(code));

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);

    size_t len = strlen(buf);
    if (len < sizeof(buf) - 2) {
        buf[len] = '\r';
        buf[len + 1] = '\n';
        buf[len + 2] = '\0';
    }

    WriteRaw(buf);
}

} // namespace hd::boot
