#include "boot_agent.h"
#include "boot_meta.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace hd::boot {

int BootAgent::Run()
{
    log_.Initialize();
    log_.Write(BootPhase::BOOTAGENT_START, "HTKIS HyperDisk X BootAgent v0.1.0");

    if (DiscoverServer() != 0) {
        if (LoadFallbackConfig()) {
            log_.Write(BootPhase::DHCP_DISCOVER, "Using fallback config (no DHCP)");
        } else {
            log_.WriteFail(BootFailCode::DHCP_FAIL, "No DHCP and no fallback config - halted");
            return -1;
        }
    }

    if (RegisterTerminal() != 0) {
        log_.Write(BootPhase::GRPC_REGISTER, "gRPC register failed, continuing with cached config");
    }

    if (DownloadBootMeta() != 0) {
        log_.WriteFail(BootFailCode::BOOTMETA_FAIL, "boot.meta download failed");
        auto& cache = BootMetaCache::Instance();
        if (cache.Load(config_.local_meta_path)) {
            log_.Write(BootPhase::CHUNKSTREAM_DL, "Using cached boot.meta");
        } else {
            log_.WriteFail(BootFailCode::BOOTMETA_FAIL, "No cached boot.meta - halted");
            return -3;
        }
    }

    if (VerifyIntegrity() != 0) {
        log_.WriteFail(BootFailCode::CRC_FAIL, "Integrity check failed - halted");
        return -4;
    }

    if (InitializeVirtualDisk() != 0) {
        log_.WriteFail(BootFailCode::BOOTMETA_FAIL, "Virtual disk init failed");
        return -5;
    }

    return TransferControl();
}

int BootAgent::DiscoverServer()
{
    log_.Write(BootPhase::DHCP_DISCOVER, "Discovering boot server via DHCP");
    DhcpClient dhcp;
    if (dhcp.Discover(config_)) {
        return 0;
    }
    return -1;
}

int BootAgent::RegisterTerminal()
{
    log_.Write(BootPhase::GRPC_REGISTER, "Registering terminal at %s:%d",
               config_.meta_server_address.c_str(), config_.meta_server_port);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        log_.WriteFail(BootFailCode::GRPC_FAIL, "WSAStartup failed");
        return -1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        log_.WriteFail(BootFailCode::GRPC_FAIL, "socket() failed");
        WSACleanup();
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.meta_server_port);
    inet_pton(AF_INET, config_.meta_server_address.c_str(), &addr.sin_addr);

    DWORD timeout = static_cast<DWORD>(config_.grpc_timeout_ms);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        log_.WriteFail(BootFailCode::GRPC_FAIL, "connect() to %s:%d failed",
                       config_.meta_server_address.c_str(), config_.meta_server_port);
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    log_.Write(BootPhase::GRPC_REGISTER, "Connected to MetadataCenter");
    closesocket(sock);
    WSACleanup();
    return 0;
}

int BootAgent::DownloadBootMeta()
{
    log_.Write(BootPhase::CHUNKSTREAM_DL, "Downloading boot.meta from %s:%d",
               config_.primary_server.c_str(), config_.primary_port);

    auto& cache = BootMetaCache::Instance();
    if (cache.LoadFromServer(config_.primary_server, config_.primary_port)) {
        cache.Save(config_.local_meta_path);
        log_.Write(BootPhase::CHUNKSTREAM_DL, "boot.meta downloaded and cached");
        return 0;
    }

    if (!config_.secondary_server.empty()) {
        log_.Write(BootPhase::CHUNKSTREAM_DL, "Primary failed, trying secondary %s:%d",
                   config_.secondary_server.c_str(), config_.secondary_port);
        if (cache.LoadFromServer(config_.secondary_server, config_.secondary_port)) {
            cache.Save(config_.local_meta_path);
            log_.Write(BootPhase::CHUNKSTREAM_DL, "boot.meta from secondary and cached");
            return 0;
        }
    }

    log_.WriteFail(BootFailCode::IMGSRV_FAIL, "All servers failed for boot.meta");
    return -1;
}

int BootAgent::VerifyIntegrity()
{
    log_.Write(BootPhase::INTEGRITY_CHECK, "Verifying boot.meta integrity");
    auto& cache = BootMetaCache::Instance();
    if (cache.IsValid()) {
        log_.Write(BootPhase::INTEGRITY_CHECK, "CRC32C OK");
        return 0;
    }
    log_.WriteFail(BootFailCode::CRC_FAIL, "CRC32C mismatch");
    return -1;
}

int BootAgent::InitializeVirtualDisk()
{
    log_.Write(BootPhase::WINPE_START, "Initializing virtual disk");
    return 0;
}

int BootAgent::TransferControl()
{
    log_.Write(BootPhase::WINPE_START, "Transferring control to WinPE");
    return 0;
}

bool BootAgent::LoadFallbackConfig()
{
    BootConfig fallback{};
    fallback.primary_server = "10.10.200.10";
    fallback.primary_port = 9527;
    fallback.meta_server_address = "10.10.300.10";
    fallback.meta_server_port = 50051;
    config_ = fallback;

    auto& cache = BootMetaCache::Instance();
    return cache.Load(config_.local_meta_path);
}

} // namespace hd::boot

int main(int argc, char* argv[])
{
    hd::boot::BootAgent agent;
    int rc = agent.Run();

    if (rc != 0) {
        hd::boot::Com1Logger::Instance().WriteFail(
            hd::boot::BootFailCode::BOOTMETA_FAIL,
            "BootAgent failed: rc=%d - Awaiting manual intervention", rc);
        Sleep(INFINITE);
    }

    return rc;
}
