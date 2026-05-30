#include "boot_agent.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace hd::boot {

static bool ParseUrl(const std::string& url, std::string& host, int& port, std::string& path)
{
    auto pos = url.find("://");
    if (pos == std::string::npos) return false;
    pos += 3;

    auto slash_pos = url.find('/', pos);
    std::string authority = (slash_pos != std::string::npos) ? url.substr(pos, slash_pos - pos) : url.substr(pos);
    path = (slash_pos != std::string::npos) ? url.substr(slash_pos) : "/";

    auto colon_pos = authority.find(':');
    if (colon_pos != std::string::npos) {
        host = authority.substr(0, colon_pos);
        port = std::stoi(authority.substr(colon_pos + 1));
    } else {
        host = authority;
        port = 80;
    }
    return true;
}

std::vector<uint8_t> HttpBootClient::DownloadScript(const std::string& url, int timeout_ms)
{
    std::vector<uint8_t> result;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return result;

    std::string host;
    int port = 80;
    std::string path;
    if (!ParseUrl(url, host, port, path)) {
        WSACleanup();
        return result;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return result; }

    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0 || !res) {
        closesocket(sock);
        WSACleanup();
        return result;
    }

    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) == SOCKET_ERROR) {
        freeaddrinfo(res);
        closesocket(sock);
        WSACleanup();
        return result;
    }
    freeaddrinfo(res);

    char req[2048];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path.c_str(), host.c_str());

    send(sock, req, req_len, 0);

    std::vector<uint8_t> response;
    response.reserve(65536);
    char buf[4096];
    int n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        response.insert(response.end(), buf, buf + n);
    }

    closesocket(sock);
    WSACleanup();

    if (response.empty()) return result;

    char* body = strstr(reinterpret_cast<char*>(response.data()), "\r\n\r\n");
    if (body) {
        body += 4;
        size_t body_len = response.size() - (body - reinterpret_cast<char*>(response.data()));
        result.assign(reinterpret_cast<uint8_t*>(body), reinterpret_cast<uint8_t*>(body) + body_len);
    }

    return result;
}

std::vector<uint8_t> HttpBootClient::DownloadFile(const std::string& url, int timeout_ms)
{
    return DownloadScript(url, timeout_ms);
}

} // namespace hd::boot
