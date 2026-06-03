#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "blackbox_types.h"

namespace hd::boot {

enum class BootPhase : uint8_t {
    PXE_START       = 0,
    IPXE_START      = 1,
    BOOTAGENT_START = 2,
    DHCP_DISCOVER   = 3,
    GRPC_REGISTER   = 4,
    CHUNKSTREAM_DL  = 5,
    INTEGRITY_CHECK = 6,
    WINPE_START     = 7,
    BOOTDIAG_RUN    = 8,
};

enum class BootFailCode : uint8_t {
    NONE            = 0,
    PXE_TIMEOUT     = 1,
    TFTP_FAIL       = 2,
    DHCP_FAIL       = 3,
    GRPC_FAIL       = 4,
    IMGSRV_FAIL     = 5,
    CRC_FAIL        = 6,
    SIGNATURE_FAIL  = 7,
    BOOTMETA_FAIL   = 8,
};

struct BootConfig {
    std::string server_address;
    uint16_t    server_port        = 8080;
    uint64_t    image_id           = 0;
    std::string boot_script_url;
    std::string image_name;
    std::string primary_server;
    uint16_t    primary_port       = 9527;
    std::string secondary_server;
    uint16_t    secondary_port     = 9527;
    std::string meta_server_address;
    uint16_t    meta_server_port   = 50051;
    std::string terminal_id;
    std::string local_meta_path    = "C:\\HyperDisk\\boot.meta";
    std::string blackbox_path      = "C:\\HyperDisk\\blackbox\\boot.bin";
    int         dhcp_timeout_ms    = 5000;
    int         grpc_timeout_ms    = 3000;
    int         dl_timeout_ms      = 10000;
    int         max_retries        = 3;
    std::string boot_id;
    std::string machine_id;
    std::string mac_address;
};

struct DhcpOption {
    uint8_t  code;
    uint8_t  length;
    std::vector<uint8_t> data;
};

using Com1LogFn = std::function<void(BootPhase phase, const char* msg)>;

class Com1Logger {
public:
    static Com1Logger& Instance();

    void Initialize();
    void Write(BootPhase phase, const char* fmt, ...);
    void WriteFail(BootFailCode code, const char* fmt, ...);
    void WriteRaw(const char* str);

private:
    Com1Logger() = default;
    void* com1_handle_ = nullptr;
    bool initialized_ = false;
};

class DhcpClient {
public:
    DhcpClient() = default;

    bool Discover(BootConfig& config);
    std::vector<DhcpOption> ParseOptions(const uint8_t* data, size_t len);

private:
    bool SendDiscover();
    bool ReceiveOffer(BootConfig& config);
};

class HttpBootClient {
public:
    HttpBootClient() = default;

    std::vector<uint8_t> DownloadScript(const std::string& url, int timeout_ms);
    std::vector<uint8_t> DownloadFile(const std::string& url, int timeout_ms);
};

class IpxeChainLoader {
public:
    IpxeChainLoader() = default;

    bool ChainLoad(const std::string& url);
};

class BootAgent {
public:
    BootAgent() = default;
    ~BootAgent() = default;

    int Run();
    void ApplyConfig(const BootConfig& cfg) { config_ = cfg; }

private:
    BootConfig config_;
    Com1Logger& log_ = Com1Logger::Instance();
    hd::diag::BlackBoxRecorder& bb_ = hd::diag::BlackBoxRecorder::Instance();
    uint64_t boot_start_ms_ = 0;

    int DiscoverServer();
    int RegisterTerminal();
    int DownloadBootMeta();
    int VerifyIntegrity();
    int InitializeVirtualDisk();
    int TransferControl();
    int ReportTelemetry(const char* phase, const char* result, uint32_t duration_ms);
    void RecordBbPhase(BootPhase phase, bool success, uint16_t latency_us);

    bool LoadFallbackConfig();
    void GenerateBootId();
    void GetMacAddress();
};

} // namespace hd::boot
