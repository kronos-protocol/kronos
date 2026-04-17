#include "malloc_wrapper.h"
#include "unity.h"
#include "kronos.h"
#include "kronos_internal.h"


static uint8_t make_valid_frame(uint8_t* buf, uint16_t buf_size, uint8_t channel, uint8_t type,
                                uint64_t packet_id, const uint8_t* body, uint16_t body_len) {
    if (buf_size < (uint16_t)(KRONOS_FRAME_HEADER_LENGTH + body_len)) return 0;
    buf[0] = 0x4B;
    buf[1] = krs_version_encode(1, 0, 0);
    buf[2] = channel;
    buf[3] = type;
    buf[4] = 0;
    buf[5] = 0;
    buf[6]  = (uint8_t)(packet_id >> 56);
    buf[7]  = (uint8_t)(packet_id >> 48);
    buf[8]  = (uint8_t)(packet_id >> 40);
    buf[9]  = (uint8_t)(packet_id >> 32);
    buf[10] = (uint8_t)(packet_id >> 24);
    buf[11] = (uint8_t)(packet_id >> 16);
    buf[12] = (uint8_t)(packet_id >> 8);
    buf[13] = (uint8_t)(packet_id & 0xFF);
    if (body && body_len > 0) {
        for (uint16_t i = 0; i < body_len; i++) buf[KRONOS_FRAME_HEADER_LENGTH + i] = body[i];
    }
    return 1;
}


void test_frame_create_valid(void) {
    uint8_t raw[KRONOS_FRAME_HEADER_LENGTH + 4];
    uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};
    make_valid_frame(raw, sizeof(raw), 5, BASIC_MESSAGE, 0x0102030405060708ULL, payload, 4);

    uint8_t body_buf[64];
    Frame_t frame = krs_frame_create(raw, sizeof(raw), body_buf, sizeof(body_buf));

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x4B, frame.protocol_char, "Protocol char should be 'K'");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(5, frame.channel, "Channel should be 5");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BASIC_MESSAGE, frame.frame_type, "Frame type mismatch");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0x0102030405060708ULL, frame.packet_id, "Packet ID mismatch");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(4, frame.body_length, "Body length should be 4");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(payload, frame.body, 4, "Body content mismatch");
}

void test_frame_create_invalid_magic(void) {
    uint8_t raw[KRONOS_FRAME_HEADER_LENGTH];
    make_valid_frame(raw, sizeof(raw), 0, 0, 0, NULL, 0);
    raw[0] = 0x00;

    uint8_t body_buf[64];
    Frame_t frame = krs_frame_create(raw, sizeof(raw), body_buf, sizeof(body_buf));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00, frame.protocol_char, "Invalid magic should yield zero frame");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, frame.channel, "Invalid magic: channel should be 0");
}

void test_frame_create_too_short(void) {
    uint8_t raw[5] = {0x4B, 1, 2, 3, 4};
    uint8_t body_buf[64];
    Frame_t frame = krs_frame_create(raw, sizeof(raw), body_buf, sizeof(body_buf));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00, frame.protocol_char, "Too-short buffer should yield zero frame");
}

void test_frame_create_null_buffer(void) {
    uint8_t body_buf[64];
    Frame_t frame = krs_frame_create(NULL, KRONOS_FRAME_HEADER_LENGTH, body_buf, sizeof(body_buf));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00, frame.protocol_char, "NULL buffer should yield zero frame");
}

void test_frame_create_heap_valid(void) {
    uint8_t raw[KRONOS_FRAME_HEADER_LENGTH + 2];
    uint8_t payload[] = {0x11, 0x22};
    make_valid_frame(raw, sizeof(raw), 3, MESSAGE_ACK, 99, payload, 2);

    Frame_t* frame = krs_frame_create_heap(raw, sizeof(raw));
    TEST_ASSERT_NOT_NULL_MESSAGE(frame, "Heap frame should not be NULL");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, frame->channel, "Channel mismatch");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(99, frame->packet_id, "Packet ID mismatch");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(payload, frame->body, 2, "Body mismatch");

    krs_frame_destroy(&frame);
    TEST_ASSERT_NULL_MESSAGE(frame, "Frame should be NULL after destroy");
}

void test_frame_create_heap_invalid(void) {
    uint8_t raw[KRONOS_FRAME_HEADER_LENGTH];
    make_valid_frame(raw, sizeof(raw), 0, 0, 0, NULL, 0);
    raw[0] = 0xFF;

    Frame_t* frame = krs_frame_create_heap(raw, sizeof(raw));
    TEST_ASSERT_NULL_MESSAGE(frame, "Invalid frame should return NULL from heap create");
}

void test_frame_builder_roundtrip(void) {
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    FrameBuilder_c* builder = krs_frame_builder_create(7, BASIC_MESSAGE);
    TEST_ASSERT_NOT_NULL_MESSAGE(builder, "Builder creation failed");

    krs_frame_builder_set_packet_id(builder, 0xCAFEBABE00000001ULL);
    krs_frame_builder_set_flag(builder, META_FLAG_ACK_REQUIRED);
    krs_frame_builder_set_data(builder, payload, 4);

    uint8_t out[KRONOS_FRAME_HEADER_LENGTH + 4];
    uint16_t written = krs_frame_builder_serialize(builder, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(sizeof(out), written, "Serialized size mismatch");

    krs_frame_builder_destroy(&builder);
    TEST_ASSERT_NULL_MESSAGE(builder, "Builder should be NULL after destroy");

    uint8_t body_buf[64];
    Frame_t frame = krs_frame_create(out, written, body_buf, sizeof(body_buf));

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x4B, frame.protocol_char, "Magic byte mismatch");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(7, frame.channel, "Channel mismatch after roundtrip");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BASIC_MESSAGE, frame.frame_type, "Frame type mismatch after roundtrip");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0xCAFEBABE00000001ULL, frame.packet_id, "Packet ID mismatch after roundtrip");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(4, frame.body_length, "Body length mismatch");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(payload, frame.body, 4, "Body content mismatch after roundtrip");

    uint16_t expected_flags = (uint16_t)(1u << META_FLAG_ACK_REQUIRED);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected_flags, frame.presence_flags, "Presence flags mismatch");
}

void test_frame_builder_serialize_too_small(void) {
    FrameBuilder_c* builder = krs_frame_builder_create(0, MESSAGE_ACK);
    TEST_ASSERT_NOT_NULL(builder);

    uint8_t out[4];
    uint16_t written = krs_frame_builder_serialize(builder, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, written, "Serialize into too-small buffer should return 0");

    krs_frame_builder_destroy(&builder);
}

void test_version_encode_decode_roundtrip(void) {
    uint8_t major = 3, minor = 5, patch = 2;
    uint8_t encoded = krs_version_encode(major, minor, patch);

    uint8_t dec_major, dec_minor, dec_patch;
    krs_version_decode(encoded, &dec_major, &dec_minor, &dec_patch);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(major, dec_major, "Major version mismatch after roundtrip");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(minor, dec_minor, "Minor version mismatch after roundtrip");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(patch, dec_patch, "Patch version mismatch after roundtrip");
}

void test_version_encode_bit_layout(void) {
    uint8_t encoded = krs_version_encode(7, 7, 3);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFF, encoded, "All-max version should encode to 0xFF");

    uint8_t encoded_zero = krs_version_encode(0, 0, 0);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00, encoded_zero, "All-zero version should encode to 0x00");
}

void test_frame_metadata_flag_count(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(5, META_FLAG_COUNT, "META_FLAG_COUNT should be 5");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, KRS_METADATA_FLAG_POSITION_SIZE[META_FLAG_ACK_REQUIRED], "ACK_REQUIRED size should be 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(4, KRS_METADATA_FLAG_POSITION_SIZE[META_FLAG_FRAGMENT_INFO], "FRAGMENT_INFO size should be 4");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(8, KRS_METADATA_FLAG_POSITION_SIZE[META_FLAG_ACK_ID], "ACK_ID size should be 8");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, KRS_METADATA_FLAG_POSITION_SIZE[META_FLAG_PRIORITY], "PRIORITY size should be 1");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(8, KRS_METADATA_FLAG_POSITION_SIZE[META_FLAG_TIMESTAMP], "TIMESTAMP size should be 8");
}
