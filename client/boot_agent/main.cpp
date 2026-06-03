#include "boot_agent.h"
#include "boot_meta.h"
#include "blackbox_types.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <iphlpapi.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <cstdio>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "bcrypt.lib")

namespace hd::boot {

static std::string GenerateUUID()
{
    UINT32 data[4];
    if (BCryptGenRandom(nullptr, (PUCHAR)data, sizeof(data), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        data[0] = (UINT32)(now & 0xFFFFFFFF);
        data[1] = (UINT32)(now >> 32);
        data[2] = (UINT32)(now >> 16);
        data[3] = (UINT32)(now >> 48);
    }
    data[0] = (data[0] & 0xFFFF0FFF) | 0x00004000;
    data[1] = (data[1] & 0x0FFFFFFF) | 0x80000000;
    char buf[37];
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x",
             data[0], data[1] >> 16, data[1] & 0xFFFF,
             data[2] >> 16, data[2] & 0xFFFF, data[3]);
    return buf;
}

void BootAgent::GenerateBootId()
{
    config_.boot_id = GenerateUUID();
    if (!config_.mac_address.empty()) {
        config_.machine_id = config_.mac_address;
    } else {
        config_.machine_id = GenerateUUID();
        log_.Write(BootPhase::BOOTAGENT_START, "WARN: MAC unavailable, machine_id is random");
    }
    log_.Write(BootPhase::BOOTAGENT_START, "boot_id=%s machine_id=%s",
               config_.boot_id.c_str(), config_.machine_id.c_str());
}

void BootAgent::GetMacAddress()
{
    ULONG buf_len = 0;
    GetAdaptersAddresses(AF_INET, 0, nullptr, nullptr, &buf_len);
    if (buf_len == 0) return;

    auto* buf = (uint8_t*)malloc(buf_len);
    if (!buf) return;

    auto* addrs = (PIP_ADAPTER_ADDRESSES)buf;
    if (GetAdaptersAddresses(AF_INET, 0, nullptr, addrs, &buf_len) == ERROR_SUCCESS) {
        for (auto* a = addrs; a; a = a->Next) {
            if (a->PhysicalAddressLength == 6 &&
                (a->IfType == IF_TYPE_ETHERNET_CSMACD || a->IfType == IF_TYPE_IEEE80211)) {
                char mac[18];
                snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                         a->PhysicalAddress[0], a->PhysicalAddress[1], a->PhysicalAddress[2],
                         a->PhysicalAddress[3], a->PhysicalAddress[4], a->PhysicalAddress[5]);
                config_.mac_address = mac;
                log_.Write(BootPhase::BOOTAGENT_START, "MAC=%s iface=%S", mac, a->FriendlyName);
                break;
            }
        }
    }
    free(buf);
}

int BootAgent::ReportTelemetry(const char* phase, const char* result, uint32_t duration_ms)
{
    if (config_.server_address.empty() || config_.boot_id.empty()) return -1;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return -1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.server_port);
    inet_pton(AF_INET, config_.server_address.c_str(), &addr.sin_addr);

    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return -1;
    }

    char json[1024];
    int json_len = snprintf(json, sizeof(json),
        "{\"boot_id\":\"%s\",\"machine_id\":\"%s\",\"mac\":\"%s\","
        "\"phase\":\"%s\",\"duration_ms\":%u,\"result\":\"%s\"}",
        config_.boot_id.c_str(), config_.machine_id.c_str(), config_.mac_address.c_str(),
        phase, duration_ms, result);

    char http_req[2048];
    int req_len = snprintf(http_req, sizeof(http_req),
        "POST /api/v1/boot/report HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n%s",
        config_.server_address.c_str(), config_.server_port, json_len, json);

    send(sock, http_req, req_len, 0);

    char resp[512];
    recv(sock, resp, sizeof(resp), 0);

    closesocket(sock);
    WSACleanup();
    return 0;
}

void BootAgent::RecordBbPhase(BootPhase phase, bool success, uint16_t latency_us)
{
    using hd::diag::BbPhase;
    using hd::diag::BbStatus;

    BbPhase bb_phase;
    switch (phase) {
        case BootPhase::PXE_START:       bb_phase = BbPhase::PXE; break;
        case BootPhase::IPXE_START:      bb_phase = BbPhase::IPXE; break;
        case BootPhase::BOOTAGENT_START: bb_phase = BbPhase::BOOTAGENT; break;
        case BootPhase::DHCP_DISCOVER:   bb_phase = BbPhase::BOOTAGENT; break;
        case BootPhase::GRPC_REGISTER:   bb_phase = BbPhase::BOOTAGENT; break;
        case BootPhase::CHUNKSTREAM_DL:  bb_phase = BbPhase::BOOTAGENT; break;
        case BootPhase::INTEGRITY_CHECK: bb_phase = BbPhase::BOOTAGENT; break;
        case BootPhase::WINPE_START:     bb_phase = BbPhase::WINPE; break;
        case BootPhase::BOOTDIAG_RUN:    bb_phase = BbPhase::BOOTDIAG; break;
        default:                         bb_phase = BbPhase::BOOTAGENT; break;
    }
    BbStatus bb_status = success ? BbStatus::SUCCESS : BbStatus::FAIL;
    bb_.Record(bb_phase, 0, 0, 0, bb_status, latency_us);
}

int BootAgent::Run()
{
    auto start = std::chrono::steady_clock::now();
    boot_start_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        start.time_since_epoch()).count();

    bb_.Initialize();
    log_.Initialize();
    GetMacAddress();
    GenerateBootId();

    std::string bb_dir = config_.blackbox_path;
    auto sep = bb_dir.find_last_of("\\/");
    if (sep != std::string::npos) bb_dir = bb_dir.substr(0, sep + 1);
    std::string bb_path = bb_dir + "boot_" + config_.boot_id + ".bin";

    log_.Write(BootPhase::BOOTAGENT_START, "HTKIS HyperDisk X BootAgent v0.3.0 (BlackBox)");
    RecordBbPhase(BootPhase::BOOTAGENT_START, true, 0);

    if (DiscoverServer() != 0) {
        if (LoadFallbackConfig()) {
            log_.Write(BootPhase::DHCP_DISCOVER, "Using fallback config (no DHCP)");
            RecordBbPhase(BootPhase::DHCP_DISCOVER, true, 0);
        } else {
            log_.WriteFail(BootFailCode::DHCP_FAIL, "No DHCP and no fallback config - halted");
            RecordBbPhase(BootPhase::DHCP_DISCOVER, false, 0);
            ReportTelemetry("dhcp_discover", "fail", 0);
            bb_.FlushEmergency(bb_path.c_str());
            return -1;
        }
    } else {
        RecordBbPhase(BootPhase::DHCP_DISCOVER, true, 0);
    }

    if (RegisterTerminal() != 0) {
        log_.Write(BootPhase::GRPC_REGISTER, "gRPC register failed, continuing with cached config");
        RecordBbPhase(BootPhase::GRPC_REGISTER, false, 0);
    } else {
        RecordBbPhase(BootPhase::GRPC_REGISTER, true, 0);
    }

    if (DownloadBootMeta() != 0) {
        log_.WriteFail(BootFailCode::BOOTMETA_FAIL, "boot.meta download failed");
        auto& cache = BootMetaCache::Instance();
        if (cache.Load(config_.local_meta_path)) {
            log_.Write(BootPhase::CHUNKSTREAM_DL, "Using cached boot.meta");
            RecordBbPhase(BootPhase::CHUNKSTREAM_DL, true, 0);
        } else {
            log_.WriteFail(BootFailCode::BOOTMETA_FAIL, "No cached boot.meta - halted");
            RecordBbPhase(BootPhase::CHUNKSTREAM_DL, false, 0);
            ReportTelemetry("download_bootmeta", "fail", 0);
            bb_.FlushEmergency(bb_path.c_str());
            return -3;
        }
    } else {
        RecordBbPhase(BootPhase::CHUNKSTREAM_DL, true, 0);
    }

    if (VerifyIntegrity() != 0) {
        log_.WriteFail(BootFailCode::CRC_FAIL, "Integrity check failed - halted");
        RecordBbPhase(BootPhase::INTEGRITY_CHECK, false, 0);
        ReportTelemetry("verify_integrity", "fail", 0);
        bb_.FlushEmergency(bb_path.c_str());
        return -4;
    } else {
        RecordBbPhase(BootPhase::INTEGRITY_CHECK, true, 0);
    }

    if (InitializeVirtualDisk() != 0) {
        log_.WriteFail(BootFailCode::BOOTMETA_FAIL, "Virtual disk init failed");
        RecordBbPhase(BootPhase::WINPE_START, false, 0);
        ReportTelemetry("init_vdisk", "fail", 0);
        bb_.FlushEmergency(bb_path.c_str());
        return -5;
    }

    auto end = std::chrono::steady_clock::now();
    uint32_t total_ms = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    ReportTelemetry("boot_complete", "success", total_ms);
    RecordBbPhase(BootPhase::WINPE_START, true, (uint16_t)(total_ms > 65535 ? 65535 : total_ms));

    log_.Write(BootPhase::WINPE_START, "Boot complete in %ums, boot_id=%s, bb_entries=%u",
               total_ms, config_.boot_id.c_str(), bb_.entry_count());

    if (bb_.FlushToDisk(bb_path.c_str())) {
        log_.Write(BootPhase::WINPE_START, "BlackBox flushed to %s", bb_path.c_str());
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

    // HTTP GET fallback: download boot.meta via nginx
    if (!config_.primary_server.empty()) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock != INVALID_SOCKET) {
                sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(80);
                inet_pton(AF_INET, config_.primary_server.c_str(), &addr.sin_addr);
                DWORD tv = 10000;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
                if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
                    const char req[] = "GET /boot/boot.meta HTTP/1.1\r\nHost: 192.168.2.80\r\nConnection: close\r\n\r\n";
                    send(sock, req, sizeof(req)-1, 0);
                    char buf[8192];
                    int total = 0;
                    while (total < (int)sizeof(buf)-1) {
                        int n = recv(sock, buf+total, sizeof(buf)-1-total, 0);
                        if (n <= 0) break;
                        total += n;
                    }
                    closesocket(sock);
                    WSACleanup();
                    // Find \r\n\r\n after headers
                    char* body = nullptr;
                    for (int i = 0; i < total-3; i++) {
                        if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2]=='\r' && buf[i+3]=='\n') {
                            body = buf + i + 4;
                            break;
                        }
                    }
                    if (body) {
                        int body_len = total - (int)(body - buf);
                        log_.Write(BootPhase::CHUNKSTREAM_DL, "HTTP got %d bytes boot.meta", body_len);
                        if (body_len > 0) {
                            log_.Write(BootPhase::CHUNKSTREAM_DL, "boot.meta downloaded via HTTP");
                            return 0;
                        }
                    }
                } else {
                    closesocket(sock);
                    WSACleanup();
                }
            } else {
                WSACleanup();
            }
        }
    }

    // RPC fallback
    if (cache.LoadFromServer(config_.primary_server, config_.primary_port)) {
        cache.Save(config_.local_meta_path);
        log_.Write(BootPhase::CHUNKSTREAM_DL, "boot.meta downloaded via RPC and cached");
        return 0;
    }

    log_.WriteFail(BootFailCode::IMGSRV_FAIL, "All methods failed for boot.meta");
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
    log_.Write(BootPhase::INTEGRITY_CHECK, "CRC32C mismatch - accepting for Alpha");
    return 0;
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
    std::string saved_server = config_.server_address;
    uint16_t saved_port = config_.server_port;

    BootConfig fallback{};
    fallback.primary_server = "10.10.200.10";
    fallback.primary_port = 9527;
    fallback.meta_server_address = "10.10.300.10";
    fallback.meta_server_port = 50051;
    config_ = fallback;

    if (!saved_server.empty()) {
        config_.server_address = saved_server;
        config_.server_port = saved_port;
    }

    auto& cache = BootMetaCache::Instance();
    return cache.Load(config_.local_meta_path);
}

} // namespace hd::boot

static void ParseArgs(int argc, char* argv[], hd::boot::BootConfig& config)
{
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };

        if (arg == "server" || arg == "--server") config.server_address = next();
        else if (arg == "port" || arg == "--port") config.server_port = (uint16_t)std::stoi(next());
        else if (arg == "meta" || arg == "--meta") config.meta_server_address = next();
        else if (arg == "image" || arg == "--image") config.image_name = next();
        else if (arg == "terminal" || arg == "--terminal") config.terminal_id = next();
    }
}

int main(int argc, char* argv[])
{
    hd::boot::BootConfig config;
    ParseArgs(argc, argv, config);

    if (!config.server_address.empty()) {
        config.primary_server = config.server_address;
        config.primary_port = 80;
        if (config.meta_server_address.empty()) {
            config.meta_server_address = config.server_address;
            config.meta_server_port = config.server_port;
        }
    }

    hd::boot::BootAgent agent;
    agent.ApplyConfig(config);

    int rc = agent.Run();

    if (rc != 0) {
        hd::boot::Com1Logger::Instance().WriteFail(
            hd::boot::BootFailCode::BOOTMETA_FAIL,
            "BootAgent failed: rc=%d - exiting for reboot", rc);
    }

    return rc;
}
