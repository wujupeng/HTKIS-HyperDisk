#include "boot_agent.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <cstring>
#include <random>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace hd::boot {

#pragma pack(push, 1)

struct DhcpPacket {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint8_t  magic[4];
};

#pragma pack(pop)

static constexpr uint8_t  DHCPDISCOVER = 1;
static constexpr uint8_t  DHCPOFFER    = 2;
static constexpr uint32_t DHCP_MAGIC   = 0x63825363;
static constexpr int      DHCP_PORT    = 67;
static constexpr int      DHCP_CLIENT_PORT = 68;

static bool GetMacAddress(uint8_t mac[6]) {
    IP_ADAPTER_INFO adapters[16];
    ULONG size = sizeof(adapters);
    if (GetAdaptersInfo(adapters, &size) != ERROR_SUCCESS) return false;

    for (auto* a = adapters; a; a = a->Next) {
        if (a->Type == MIB_IF_TYPE_ETHERNET && a->AddressLength == 6) {
            memcpy(mac, a->Address, 6);
            return true;
        }
    }
    return false;
}

bool DhcpClient::Discover(BootConfig& config)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "WSAStartup failed: %d", WSAGetLastError());
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "socket() failed");
        WSACleanup();
        return false;
    }

    BOOL broadcast = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

    DWORD timeout = static_cast<DWORD>(config.dhcp_timeout_ms);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    sockaddr_in client_addr{};
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(DHCP_CLIENT_PORT);
    client_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&client_addr), sizeof(client_addr)) == SOCKET_ERROR) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "bind() failed: %d", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return false;
    }

    DhcpPacket discover{};
    discover.op = DHCPDISCOVER;
    discover.htype = 1;
    discover.hlen = 6;
    discover.flags = htons(0x8000);
    std::random_device rd;
    discover.xid = rd();

    uint8_t mac[6]{};
    GetMacAddress(mac);
    memcpy(discover.chaddr, mac, 6);
    discover.magic[0] = (DHCP_MAGIC >> 24) & 0xFF;
    discover.magic[1] = (DHCP_MAGIC >> 16) & 0xFF;
    discover.magic[2] = (DHCP_MAGIC >> 8) & 0xFF;
    discover.magic[3] = DHCP_MAGIC & 0xFF;

    std::vector<uint8_t> buf(sizeof(DhcpPacket) + 256, 0);
    memcpy(buf.data(), &discover, sizeof(DhcpPacket));

    size_t opt_off = sizeof(DhcpPacket);
    buf[opt_off++] = 53; buf[opt_off++] = 1; buf[opt_off++] = DHCPDISCOVER;
    buf[opt_off++] = 12;
    buf[opt_off++] = 8;
    const char* hostname = "HYPERDISK";
    for (int i = 0; i < 8; i++) buf[opt_off++] = static_cast<uint8_t>(hostname[i]);
    buf[opt_off++] = 55; buf[opt_off++] = 4;
    buf[opt_off++] = 1;  buf[opt_off++] = 3;
    buf[opt_off++] = 6;  buf[opt_off++] = 60;
    buf[opt_off++] = 255;

    sockaddr_in bcast{};
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(DHCP_PORT);
    bcast.sin_addr.s_addr = INADDR_BROADCAST;

    Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "Sending DHCP DISCOVER (xid=0x%08X)", discover.xid);

    if (sendto(sock, reinterpret_cast<const char*>(buf.data()), static_cast<int>(opt_off), 0,
               reinterpret_cast<sockaddr*>(&bcast), sizeof(bcast)) == SOCKET_ERROR) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "sendto() failed: %d", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return false;
    }

    std::vector<uint8_t> recv_buf(1500);
    sockaddr_in from{};
    int from_len = sizeof(from);

    int n = recvfrom(sock, reinterpret_cast<char*>(recv_buf.data()), static_cast<int>(recv_buf.size()), 0,
                     reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n <= 0) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "No DHCP response (timeout %dms)", config.dhcp_timeout_ms);
        closesocket(sock);
        WSACleanup();
        return false;
    }

    if (static_cast<size_t>(n) < sizeof(DhcpPacket)) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "Response too short: %d", n);
        closesocket(sock);
        WSACleanup();
        return false;
    }

    DhcpPacket* offer = reinterpret_cast<DhcpPacket*>(recv_buf.data());
    if (offer->op != DHCPOFFER) {
        Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "Not a DHCPOFFER: op=%d", offer->op);
        closesocket(sock);
        WSACleanup();
        return false;
    }

    uint8_t* opts = recv_buf.data() + sizeof(DhcpPacket);
    size_t opts_len = n - sizeof(DhcpPacket);
    auto options = ParseOptions(opts, opts_len);

    for (const auto& opt : options) {
        switch (opt.code) {
            case 54: {
                if (opt.length >= 4) {
                    uint32_t siaddr;
                    memcpy(&siaddr, opt.data.data(), 4);
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &siaddr, ip, sizeof(ip));
                    config.server_address = ip;
                    if (config.primary_server.empty()) {
                        config.primary_server = ip;
                    }
                    Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "Server: %s", ip);
                }
                break;
            }
            case 67: {
                std::string bootfile(opt.data.begin(), opt.data.end());
                config.boot_script_url = bootfile;
                Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "Boot file: %s", bootfile.c_str());
                break;
            }
            case 1: {
                if (opt.length >= 4) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, opt.data.data(), ip, sizeof(ip));
                    Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "Netmask: %s", ip);
                }
                break;
            }
            case 3: {
                if (opt.length >= 4) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, opt.data.data(), ip, sizeof(ip));
                    Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "Gateway: %s", ip);
                }
                break;
            }
        }
    }

    char offered_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &offer->yiaddr, offered_ip, sizeof(offered_ip));
    Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "Offered IP: %s", offered_ip);

    closesocket(sock);
    WSACleanup();

    if (!config.server_address.empty()) {
        Com1Logger::Instance().Write(BootPhase::DHCP_DISCOVER, "OK - server=%s", config.server_address.c_str());
        return true;
    }

    Com1Logger::Instance().WriteFail(BootFailCode::DHCP_FAIL, "No server address in DHCPOFFER");
    return false;
}

std::vector<DhcpOption> DhcpClient::ParseOptions(const uint8_t* data, size_t len) {
    std::vector<DhcpOption> options;
    size_t offset = 0;

    while (offset + 2 <= len) {
        DhcpOption opt;
        opt.code = data[offset++];
        if (opt.code == 0xFF) break;
        if (opt.code == 0) continue;
        opt.length = data[offset++];
        if (offset + opt.length > len) break;
        opt.data.assign(data + offset, data + offset + opt.length);
        offset += opt.length;
        options.push_back(std::move(opt));
    }

    return options;
}

} // namespace hd::boot
