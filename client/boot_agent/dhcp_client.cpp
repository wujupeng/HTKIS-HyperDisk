#include "boot_agent.h"

namespace hd::boot {

bool DhcpClient::Discover(BootConfig&)
{
    return true;
}

std::vector<DhcpOption> DhcpClient::ParseOptions(const uint8_t* data, size_t len)
{
    std::vector<DhcpOption> options;
    size_t offset = 0;

    while (offset + 2 <= len) {
        DhcpOption opt;
        opt.code = data[offset++];
        if (opt.code == 0xFF) break;
        opt.length = data[offset++];
        if (offset + opt.length > len) break;
        opt.data.assign(data + offset, data + offset + opt.length);
        offset += opt.length;
        options.push_back(std::move(opt));
    }

    return options;
}

std::vector<uint8_t> HttpBootClient::DownloadScript(const std::string&, int)
{
    return {};
}

bool IpxeChainLoader::ChainLoad(const std::string&)
{
    return true;
}

} // namespace hd::boot
