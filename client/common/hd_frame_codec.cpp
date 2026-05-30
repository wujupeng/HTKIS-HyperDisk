#include "hd_frame_codec.h"

namespace hd::proto {

static uint32_t crc32c_table[256];
static bool crc32c_initialized = false;

static void EnsureCrc32cInit() {
    if (crc32c_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0x82F63B78) : (crc >> 1);
        }
        crc32c_table[i] = crc;
    }
    crc32c_initialized = true;
}

uint32_t FrameCodec::ComputeCrc32C(const uint8_t* data, uint32_t len) {
    EnsureCrc32cInit();
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32c_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

void FrameCodec::EncodeHeader(FrameHeader& hdr, Opcode opcode, uint8_t flags,
                               uint32_t request_id, uint32_t payload_len,
                               uint64_t image_id, uint64_t block_offset,
                               uint32_t block_count, uint8_t layer_id) {
    hdr.magic = FRAME_MAGIC;
    hdr.version = FRAME_VERSION;
    hdr.opcode = static_cast<uint8_t>(opcode);
    hdr.flags = flags;
    hdr.request_id = request_id;
    hdr.payload_len = payload_len;
    hdr.image_id = image_id;
    hdr.block_offset = block_offset;
    hdr.block_count = block_count;
    hdr.layer_id = layer_id;
    hdr.reserved[0] = 0;
    hdr.reserved[1] = 0;
    hdr.reserved[2] = 0;
}

bool FrameCodec::ValidateHeader(const FrameHeader& hdr) {
    if (hdr.magic != FRAME_MAGIC) return false;
    if (hdr.version != FRAME_VERSION) return false;
    if (hdr.opcode == 0) return false;
    return true;
}

uint32_t FrameCodec::ComputeFrameCrc(const FrameHeader& hdr) {
    return ComputeCrc32C(reinterpret_cast<const uint8_t*>(&hdr), FRAME_HDR_SIZE);
}

bool FrameCodec::EncodeFrame(uint8_t* buffer, uint32_t buf_size,
                              const FrameHeader& hdr,
                              const uint8_t* payload, uint32_t payload_len,
                              uint32_t& total_len) {
    total_len = FRAME_HDR_SIZE + payload_len;
    if (buf_size < total_len) return false;

    memcpy(buffer, &hdr, FRAME_HDR_SIZE);
    if (payload && payload_len > 0) {
        memcpy(buffer + FRAME_HDR_SIZE, payload, payload_len);
    }

    return true;
}

bool FrameCodec::DecodeFrame(const uint8_t* buffer, uint32_t buf_len,
                              FrameHeader& hdr,
                              uint8_t* payload, uint32_t max_payload_len,
                              uint32_t& payload_len) {
    if (buf_len < FRAME_HDR_SIZE) return false;

    memcpy(&hdr, buffer, FRAME_HDR_SIZE);
    if (!ValidateHeader(hdr)) return false;

    payload_len = hdr.payload_len;
    if (buf_len < FRAME_HDR_SIZE + payload_len) return false;
    if (payload_len > max_payload_len) return false;

    if (payload && payload_len > 0) {
        memcpy(payload, buffer + FRAME_HDR_SIZE, payload_len);
    }

    return true;
}

const char* FrameCodec::OpcodeName(Opcode op) {
    switch (op) {
        case Opcode::BLOCK_READ_REQ:    return "BLOCK_READ_REQ";
        case Opcode::BLOCK_READ_RSP:    return "BLOCK_READ_RSP";
        case Opcode::BLOCK_WRITE_REQ:   return "BLOCK_WRITE_REQ";
        case Opcode::BLOCK_WRITE_RSP:   return "BLOCK_WRITE_RSP";
        case Opcode::HEARTBEAT:         return "HEARTBEAT";
        case Opcode::HEARTBEAT_ACK:     return "HEARTBEAT_ACK";
        case Opcode::CACHE_INVALIDATE:  return "CACHE_INVALIDATE";
        case Opcode::PREFETCH_PUSH:     return "PREFETCH_PUSH";
        case Opcode::VECTOR_READ_REQ:   return "VECTOR_READ_REQ";
        case Opcode::VECTOR_READ_RSP:   return "VECTOR_READ_RSP";
        case Opcode::VECTOR_WRITE_REQ:  return "VECTOR_WRITE_REQ";
        case Opcode::VECTOR_WRITE_RSP:  return "VECTOR_WRITE_RSP";
        case Opcode::ERROR_RSP:         return "ERROR_RSP";
        default:                        return "UNKNOWN";
    }
}

} // namespace hd::proto
