#pragma once

#include <cstdint>
#include <cstring>

namespace hd::proto {

constexpr uint32_t FRAME_MAGIC    = 0x48444B47;
constexpr uint16_t FRAME_VERSION  = 0x0001;
constexpr uint32_t FRAME_HDR_SIZE = 40;

enum class Opcode : uint8_t {
    BLOCK_READ_REQ    = 0x01,
    BLOCK_READ_RSP    = 0x02,
    BLOCK_WRITE_REQ   = 0x03,
    BLOCK_WRITE_RSP   = 0x04,
    HEARTBEAT         = 0x10,
    HEARTBEAT_ACK     = 0x11,
    CACHE_INVALIDATE  = 0x20,
    PREFETCH_PUSH     = 0x30,
    VECTOR_READ_REQ   = 0x40,
    VECTOR_READ_RSP   = 0x41,
    VECTOR_WRITE_REQ  = 0x42,
    VECTOR_WRITE_RSP  = 0x43,
    ERROR_RSP         = 0xFF,
};

constexpr uint8_t FLAG_COMPRESSED = 0x01;
constexpr uint8_t FLAG_URGENT     = 0x02;
constexpr uint8_t FLAG_CACHED     = 0x04;

#pragma pack(push, 1)

struct FrameHeader {
    uint32_t  magic;
    uint16_t  version;
    uint8_t   opcode;
    uint8_t   flags;
    uint32_t  request_id;
    uint32_t  payload_len;
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    uint8_t   layer_id;
    uint8_t   reserved[3];
};

static_assert(sizeof(FrameHeader) == 40, "FrameHeader must be 40 bytes");

struct BlockReadReqPayload {
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    uint8_t   layer_id;
};

struct BlockReadRspPayload {
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    int32_t   status;
};

struct BlockWriteReqPayload {
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    uint8_t   layer_id;
    uint8_t   policy;
};

struct BlockWriteRspPayload {
    uint64_t  image_id;
    uint64_t  block_offset;
    int32_t   status;
};

struct HeartbeatPayload {
    uint64_t  terminal_id;
    uint64_t  timestamp;
    uint32_t  cache_hit_rate;
    uint32_t  pending_io;
};

struct HeartbeatAckPayload {
    uint64_t  server_timestamp;
    uint32_t  active_connections;
    uint32_t  server_load;
};

struct ErrorRspPayload {
    int32_t   error_code;
    char      error_msg[128];
};

#pragma pack(pop)

class FrameCodec {
public:
    static void EncodeHeader(FrameHeader& hdr, Opcode opcode, uint8_t flags,
                             uint32_t request_id, uint32_t payload_len,
                             uint64_t image_id, uint64_t block_offset,
                             uint32_t block_count, uint8_t layer_id);

    static bool ValidateHeader(const FrameHeader& hdr);

    static uint32_t ComputeCrc32C(const uint8_t* data, uint32_t len);
    static uint32_t ComputeFrameCrc(const FrameHeader& hdr);

    static bool EncodeFrame(uint8_t* buffer, uint32_t buf_size,
                            const FrameHeader& hdr,
                            const uint8_t* payload, uint32_t payload_len,
                            uint32_t& total_len);

    static bool DecodeFrame(const uint8_t* buffer, uint32_t buf_len,
                            FrameHeader& hdr,
                            uint8_t* payload, uint32_t max_payload_len,
                            uint32_t& payload_len);

    static const char* OpcodeName(Opcode op);
};

} // namespace hd::proto
