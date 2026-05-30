#pragma once

#include <cstdint>
#include <cstdlib>

#define HD_BLOCK_SIZE       4096
#define HD_CHUNK_SIZE       (64 * 1024)
#define HD_FRAME_HDR_SIZE   32
#define HD_MAX_RETRY        3
#define HD_DEFAULT_TIMEOUT  10000

#define HD_MAGIC            0x48444B47
#define HD_VERSION          0x0001

namespace hd {

enum class FrameType : uint8_t {
    BLOCK_READ_REQ    = 0x01,
    BLOCK_READ_RSP    = 0x02,
    BLOCK_WRITE_REQ   = 0x03,
    BLOCK_WRITE_RSP   = 0x04,
    HEARTBEAT         = 0x10,
    HEARTBEAT_ACK     = 0x11,
    CACHE_INVALIDATE  = 0x20,
    PREFETCH_PUSH     = 0x30,
    ERROR_RSP         = 0xFF,
};

enum class LayerType : uint8_t {
    OS      = 0,
    DRIVER  = 1,
    APP     = 2,
    OVERLAY = 3,
};

enum class WritePolicy : uint8_t {
    RAM_OVERLAY         = 0,
    NVME_OVERLAY        = 1,
    PERSISTENT_USER     = 2,
    READ_ONLY           = 3,
    SNAPSHOT            = 4,
    HYBRID              = 5,
};

enum class QoSLevel : uint8_t {
    P0_VDISK_IO  = 0,
    P1_WRITEBACK = 1,
    P2_PREFETCH  = 2,
    P3_UPDATE    = 3,
};

#pragma pack(push, 1)

struct FrameHeader {
    uint32_t  magic;
    uint16_t  version;
    uint8_t   frame_type;
    uint8_t   flags;
    uint32_t  frame_id;
    uint32_t  payload_len;
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    uint8_t   layer_id;
    uint8_t   reserved[3];
};

struct BlockReadReq {
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    uint8_t   layer_id;
};

struct BlockReadRsp {
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    int32_t   status;
};

struct BlockWriteReq {
    uint64_t  image_id;
    uint64_t  block_offset;
    uint32_t  block_count;
    uint8_t   layer_id;
    uint8_t   policy;
};

struct BlockWriteRsp {
    uint64_t  image_id;
    uint64_t  block_offset;
    int32_t   status;
};

struct HeartbeatMsg {
    uint64_t  terminal_id;
    uint64_t  timestamp;
    uint32_t  cache_hit_rate;
    uint32_t  pending_io;
};

#pragma pack(pop)

} // namespace hd
