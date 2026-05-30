#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace hd::boot {

struct BootConfig {
    std::string server_address;
    uint16_t    server_port;
    uint64_t    image_id;
    std::string boot_script_url;
    std::string image_name;
};

struct DhcpOption {
    uint8_t  code;
    uint8_t  length;
    std::vector<uint8_t> data;
};

class BootAgent {
public:
    BootAgent() = default;
    ~BootAgent() = default;

    int Run();

private:
    BootConfig config_;

    int DiscoverServer();
    int DownloadBootScript();
    bool VerifyIntegrity(const std::vector<uint8_t>& data, const std::string& expected_hash);
    int InitializeVirtualDisk();
    int TransferControl();
};

class DhcpClient {
public:
    DhcpClient() = default;

    bool Discover(BootConfig& config);
    std::vector<DhcpOption> ParseOptions(const uint8_t* data, size_t len);
};

class HttpBootClient {
public:
    HttpBootClient() = default;

    std::vector<uint8_t> DownloadScript(const std::string& url, int timeout_ms);
};

class IpxeChainLoader {
public:
    IpxeChainLoader() = default;

    bool ChainLoad(const std::string& url);
};

} // namespace hd::boot
