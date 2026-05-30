#include "boot_meta.h"
#include "hd_frame_codec.h"
#include <cstring>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace hd::boot {

static uint32_t Crc32c(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0x82F63B78 : 0);
    }
    return crc ^ 0xFFFFFFFF;
}

BootMetaCache& BootMetaCache::Instance() {
    static BootMetaCache instance;
    return instance;
}

uint32_t BootMetaCache::ComputeCrc() const {
    size_t crc_offset = offsetof(BootMeta, crc32c);
    return Crc32c(reinterpret_cast<const uint8_t*>(&meta_), crc_offset);
}

bool BootMetaCache::ValidateCrc() {
    uint32_t expected = ComputeCrc();
    return meta_.crc32c == expected;
}

void BootMetaCache::ApplyDefaults() {
    std::memset(&meta_, 0, sizeof(BootMeta));
    meta_.magic = BOOT_META_MAGIC;
    meta_.version = BOOT_META_VERSION;
    meta_.block_size = 4096;
    meta_.write_policy = 0;
    std::strncpy(meta_.primary_server, "10.10.200.10", sizeof(meta_.primary_server) - 1);
    meta_.primary_port = 9527;
}

bool BootMetaCache::Load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        ApplyDefaults();
        valid_ = false;
        return false;
    }

    f.read(reinterpret_cast<char*>(&meta_), sizeof(BootMeta));
    if (!f || meta_.magic != BOOT_META_MAGIC) {
        ApplyDefaults();
        valid_ = false;
        return false;
    }

    if (!ValidateCrc()) {
        ApplyDefaults();
        valid_ = false;
        return false;
    }

    valid_ = true;
    return true;
}

bool BootMetaCache::Save(const std::string& path) {
    meta_.magic = BOOT_META_MAGIC;
    meta_.version = BOOT_META_VERSION;
    meta_.crc32c = ComputeCrc();

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    f.write(reinterpret_cast<const char*>(&meta_), sizeof(BootMeta));
    return f.good();
}

bool BootMetaCache::LoadFromServer(const std::string& server, uint16_t port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server.c_str(), &addr.sin_addr);

    DWORD timeout = 10000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    using namespace hd::proto;

    FrameHeader hdr{};
    FrameCodec::EncodeHeader(
        hdr,
        Opcode::BLOCK_READ_REQ,
        0,
        1,
        sizeof(BlockReadReqPayload),
        meta_.image_id,
        0,
        1,
        meta_.layer_id
    );

    uint8_t send_buf[FRAME_HDR_SIZE + sizeof(BlockReadReqPayload) + 4];
    uint32_t total_len = 0;
    BlockReadReqPayload req{};
    req.image_id = meta_.image_id;
    req.block_offset = 0;
    req.block_count = 1;
    req.layer_id = meta_.layer_id;

    if (!FrameCodec::EncodeFrame(send_buf, sizeof(send_buf), hdr,
            reinterpret_cast<const uint8_t*>(&req), sizeof(req), total_len)) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    if (send(sock, reinterpret_cast<const char*>(send_buf), total_len, 0) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    uint8_t recv_buf[65536];
    int n = recv(sock, reinterpret_cast<char*>(recv_buf), sizeof(recv_buf), 0);
    closesocket(sock);
    WSACleanup();

    if (n <= 0) return false;

    FrameHeader rsp_hdr{};
    uint8_t payload[65536];
    uint32_t payload_len = 0;
    if (!FrameCodec::DecodeFrame(recv_buf, static_cast<uint32_t>(n), rsp_hdr, payload, sizeof(payload), payload_len)) {
        return false;
    }

    if (rsp_hdr.opcode == static_cast<uint8_t>(Opcode::BLOCK_READ_RSP) && payload_len > 0) {
        auto* rsp = reinterpret_cast<BlockReadRspPayload*>(payload);
        if (rsp->status == 0 && payload_len > sizeof(BlockReadRspPayload)) {
            size_t data_len = payload_len - sizeof(BlockReadRspPayload);
            if (data_len >= sizeof(BootMeta)) {
                memcpy(&meta_, payload + sizeof(BlockReadRspPayload), sizeof(BootMeta));
                valid_ = (meta_.magic == BOOT_META_MAGIC) && ValidateCrc();
                return valid_;
            }
        }
    }

    return false;
}

std::string BootMetaCache::PrimaryServer() const {
    return std::string(meta_.primary_server, strnlen(meta_.primary_server, sizeof(meta_.primary_server)));
}

std::string BootMetaCache::SecondaryServer() const {
    return std::string(meta_.secondary_server, strnlen(meta_.secondary_server, sizeof(meta_.secondary_server)));
}

} // namespace hd::boot
