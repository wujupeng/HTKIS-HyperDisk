#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "hd_frame_codec.h"

using namespace hd::proto;

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { test_count++; printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { pass_count++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((a) != (b)) { FAIL(msg); return; } } while(0)
#define ASSERT_TRUE(a, msg) do { if (!(a)) { FAIL(msg); return; } } while(0)

static void test_frame_header_size() {
    TEST("FrameHeader size = 40 bytes");
    ASSERT_EQ(sizeof(FrameHeader), 40u, "FrameHeader must be 40 bytes");
    PASS();
}

static void test_encode_header() {
    TEST("EncodeHeader fills all fields correctly");
    FrameHeader hdr{};
    FrameCodec::EncodeHeader(hdr, Opcode::BLOCK_READ_REQ, 0, 42, 24, 1, 0, 1, 0);
    ASSERT_EQ(hdr.magic, FRAME_MAGIC, "magic mismatch");
    ASSERT_EQ(hdr.version, FRAME_VERSION, "version mismatch");
    ASSERT_EQ(hdr.opcode, static_cast<uint8_t>(Opcode::BLOCK_READ_REQ), "opcode mismatch");
    ASSERT_EQ(hdr.flags, 0u, "flags mismatch");
    ASSERT_EQ(hdr.request_id, 42u, "request_id mismatch");
    ASSERT_EQ(hdr.payload_len, 24u, "payload_len mismatch");
    ASSERT_EQ(hdr.image_id, 1ULL, "image_id mismatch");
    ASSERT_EQ(hdr.block_offset, 0ULL, "block_offset mismatch");
    ASSERT_EQ(hdr.block_count, 1u, "block_count mismatch");
    ASSERT_EQ(hdr.layer_id, 0u, "layer_id mismatch");
    PASS();
}

static void test_validate_header() {
    TEST("ValidateHeader accepts valid header");
    FrameHeader hdr{};
    FrameCodec::EncodeHeader(hdr, Opcode::HEARTBEAT, 0, 1, 0, 0, 0, 0, 0);
    ASSERT_TRUE(FrameCodec::ValidateHeader(hdr), "valid header rejected");
    PASS();
}

static void test_validate_header_bad_magic() {
    TEST("ValidateHeader rejects bad magic");
    FrameHeader hdr{};
    FrameCodec::EncodeHeader(hdr, Opcode::HEARTBEAT, 0, 1, 0, 0, 0, 0, 0);
    hdr.magic = 0xDEADBEEF;
    ASSERT_TRUE(!FrameCodec::ValidateHeader(hdr), "bad magic accepted");
    PASS();
}

static void test_encode_decode_roundtrip() {
    TEST("Encode/Decode roundtrip preserves data");
    FrameHeader hdr{};
    BlockReadReqPayload req{};
    req.image_id = 123;
    req.block_offset = 456;
    req.block_count = 8;
    req.layer_id = 1;

    uint32_t payload_len = sizeof(BlockReadReqPayload);
    FrameCodec::EncodeHeader(hdr, Opcode::BLOCK_READ_REQ, FLAG_COMPRESSED, 99,
                              payload_len, req.image_id, req.block_offset, req.block_count, req.layer_id);

    uint8_t buffer[256];
    uint32_t total_len = 0;
    bool ok = FrameCodec::EncodeFrame(buffer, sizeof(buffer), hdr,
                                        reinterpret_cast<const uint8_t*>(&req), payload_len, total_len);
    ASSERT_TRUE(ok, "EncodeFrame failed");
    ASSERT_EQ(total_len, FRAME_HDR_SIZE + payload_len, "total_len mismatch");

    FrameHeader dec_hdr{};
    uint8_t dec_payload[256];
    uint32_t dec_payload_len = 0;
    ok = FrameCodec::DecodeFrame(buffer, total_len, dec_hdr,
                                  dec_payload, sizeof(dec_payload), dec_payload_len);
    ASSERT_TRUE(ok, "DecodeFrame failed");
    ASSERT_EQ(dec_hdr.magic, FRAME_MAGIC, "decoded magic mismatch");
    ASSERT_EQ(dec_hdr.opcode, static_cast<uint8_t>(Opcode::BLOCK_READ_REQ), "decoded opcode mismatch");
    ASSERT_EQ(dec_hdr.request_id, 99u, "decoded request_id mismatch");
    ASSERT_EQ(dec_hdr.flags, FLAG_COMPRESSED, "decoded flags mismatch");
    ASSERT_EQ(dec_payload_len, payload_len, "decoded payload_len mismatch");

    BlockReadReqPayload dec_req{};
    memcpy(&dec_req, dec_payload, sizeof(dec_req));
    ASSERT_EQ(dec_req.image_id, 123ULL, "decoded image_id mismatch");
    ASSERT_EQ(dec_req.block_offset, 456ULL, "decoded block_offset mismatch");
    ASSERT_EQ(dec_req.block_count, 8u, "decoded block_count mismatch");
    PASS();
}

static void test_crc32c() {
    TEST("CRC32C computation");
    const char* data = "123456789";
    uint32_t crc = FrameCodec::ComputeCrc32C(reinterpret_cast<const uint8_t*>(data), 9);
    ASSERT_EQ(crc, 0xE3069283u, "CRC32C mismatch for '123456789'");
    PASS();
}

static void test_opcode_name() {
    TEST("OpcodeName returns correct strings");
    ASSERT_EQ(strcmp(FrameCodec::OpcodeName(Opcode::BLOCK_READ_REQ), "BLOCK_READ_REQ"), 0, "BLOCK_READ_REQ name mismatch");
    ASSERT_EQ(strcmp(FrameCodec::OpcodeName(Opcode::HEARTBEAT), "HEARTBEAT"), 0, "HEARTBEAT name mismatch");
    ASSERT_EQ(strcmp(FrameCodec::OpcodeName(Opcode::ERROR_RSP), "ERROR_RSP"), 0, "ERROR_RSP name mismatch");
    PASS();
}

static void test_decode_buffer_too_small() {
    TEST("DecodeFrame rejects buffer too small");
    uint8_t tiny_buf[16] = {0};
    FrameHeader hdr{};
    uint8_t payload[64];
    uint32_t payload_len = 0;
    bool ok = FrameCodec::DecodeFrame(tiny_buf, 16, hdr, payload, 64, payload_len);
    ASSERT_TRUE(!ok, "DecodeFrame accepted too-small buffer");
    PASS();
}

int main() {
    printf("=== hd_frame_codec unit tests ===\n\n");

    test_frame_header_size();
    test_encode_header();
    test_validate_header();
    test_validate_header_bad_magic();
    test_encode_decode_roundtrip();
    test_crc32c();
    test_opcode_name();
    test_decode_buffer_too_small();

    printf("\n%d/%d tests passed\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
