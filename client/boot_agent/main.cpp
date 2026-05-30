#include "boot_agent.h"
#include <iostream>

namespace hd::boot {

int BootAgent::Run()
{
    if (DiscoverServer() != 0) {
        std::cerr << "Failed to discover boot server" << std::endl;
        return -1;
    }

    if (DownloadBootScript() != 0) {
        std::cerr << "Failed to download boot script" << std::endl;
        return -2;
    }

    if (InitializeVirtualDisk() != 0) {
        std::cerr << "Failed to initialize virtual disk" << std::endl;
        return -3;
    }

    return TransferControl();
}

int BootAgent::DiscoverServer()
{
    DhcpClient dhcp;
    return dhcp.Discover(config_) ? 0 : -1;
}

int BootAgent::DownloadBootScript()
{
    HttpBootClient http;
    auto script = http.DownloadScript(config_.boot_script_url, 3000);
    if (script.empty()) {
        return -1;
    }
    return 0;
}

bool BootAgent::VerifyIntegrity(const std::vector<uint8_t>&, const std::string&)
{
    return true;
}

int BootAgent::InitializeVirtualDisk()
{
    return 0;
}

int BootAgent::TransferControl()
{
    return 0;
}

} // namespace hd::boot

int main(int argc, char* argv[])
{
    hd::boot::BootAgent agent;
    return agent.Run();
}
