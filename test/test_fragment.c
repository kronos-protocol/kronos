#include "kronos_fragment.h"
#include "fragment_internal.h"
#include "frame_metadata.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <string.h>
#include <stdlib.h>


void test_fragment_split_no_frag_when_fits(void) {
    uint8_t data[100];
    memset(data, 0xAA, sizeof(data));

    FragmentResult_t result = krs_fragment_split(1, BASIC_MESSAGE, 42, data, sizeof(data), KRS_DEFAULT_MTU, 0);

    TEST_ASSERT_TRUE(result.base.valid);
    TEST_ASSERT_EQUAL_UINT16(1, result.fragment_count);
    TEST_ASSERT_NOT_NULL(result.fragments);
    TEST_ASSERT_NOT_NULL(result.fragments[0]);

    uint8_t body_buf[200];
    Frame_t frame = krs_frame_create(result.fragments[0], result.fragment_sizes[0], body_buf, sizeof(body_buf));
    TEST_ASSERT_EQUAL_HEX8(0x4B, frame.protocol_char);
    TEST_ASSERT_EQUAL_UINT16(100, frame.body_length);
    TEST_ASSERT_EQUAL_UINT8(0, (frame.presence_flags >> META_FLAG_FRAGMENT_INFO) & 1);

    krs_fragment_result_destroy(&result);
    TEST_ASSERT_NULL(result.fragments);
    TEST_ASSERT_EQUAL_UINT16(0, result.fragment_count);
}

void test_fragment_split_correct_count(void) {
    uint32_t data_len = (uint32_t)KRS_MAX_PAYLOAD_PER_FRAGMENT * 2 + 10;
    uint8_t* data = malloc(data_len);
    memset(data, 0x55, data_len);

    FragmentResult_t result = krs_fragment_split(1, BASIC_MESSAGE, 99, data, data_len, KRS_DEFAULT_MTU, 0);

    TEST_ASSERT_TRUE(result.base.valid);
    TEST_ASSERT_EQUAL_UINT16(3, result.fragment_count);
    TEST_ASSERT_NOT_NULL(result.fragments);
    TEST_ASSERT_NOT_NULL(result.fragment_sizes);

    for (uint16_t i = 0; i < result.fragment_count; i++) {
        TEST_ASSERT_NOT_NULL(result.fragments[i]);
    }

    krs_fragment_result_destroy(&result);
    free(data);
}

void test_fragment_split_fragment_info_flag_set(void) {
    uint32_t data_len = (uint32_t)(KRS_DEFAULT_MTU - KRONOS_FRAME_HEADER_LENGTH) + 1;
    uint8_t* data = malloc(data_len);
    memset(data, 0xBB, data_len);

    FragmentResult_t result = krs_fragment_split(2, BASIC_MESSAGE, 77, data, data_len, KRS_DEFAULT_MTU, 0);

    TEST_ASSERT_TRUE(result.base.valid);
    TEST_ASSERT_EQUAL_UINT16(2, result.fragment_count);

    uint8_t body_buf[KRS_DEFAULT_MTU];
    Frame_t f0 = krs_frame_create(result.fragments[0], result.fragment_sizes[0], body_buf, sizeof(body_buf));
    TEST_ASSERT_NOT_EQUAL_UINT16(0, f0.presence_flags & (uint16_t)(1u << META_FLAG_FRAGMENT_INFO));

    krs_fragment_result_destroy(&result);
    free(data);
}

void test_fragment_reassemble_in_order(void) {
    uint32_t data_len = (uint32_t)KRS_MAX_PAYLOAD_PER_FRAGMENT * 2 + 100;
    uint8_t* data = malloc(data_len);
    for (uint32_t i = 0; i < data_len; i++) data[i] = (uint8_t)(i & 0xFF);

    FragmentResult_t split = krs_fragment_split(1, BASIC_MESSAGE, 77, data, data_len, KRS_DEFAULT_MTU, 0);
    TEST_ASSERT_TRUE(split.base.valid);
    TEST_ASSERT_EQUAL_UINT16(3, split.fragment_count);

    Reassembler_t* reassembler = krs_reassembler_create();
    TEST_ASSERT_NOT_NULL(reassembler);

    uint8_t body_buf[KRS_DEFAULT_MTU];
    ReassembleResult_t r;

    for (uint16_t i = 0; i < split.fragment_count - 1; i++) {
        Frame_t frame = krs_frame_create(split.fragments[i], split.fragment_sizes[i], body_buf, sizeof(body_buf));
        r = krs_reassembler_feed(reassembler, &frame);
        TEST_ASSERT_FALSE(r.complete);
        TEST_ASSERT_NULL(r.data);
    }

    Frame_t last = krs_frame_create(split.fragments[split.fragment_count - 1],
                                    split.fragment_sizes[split.fragment_count - 1],
                                    body_buf, sizeof(body_buf));
    r = krs_reassembler_feed(reassembler, &last);
    TEST_ASSERT_TRUE(r.complete);
    TEST_ASSERT_NOT_NULL(r.data);
    TEST_ASSERT_EQUAL_UINT32(data_len, r.data_length);
    TEST_ASSERT_EQUAL_MEMORY(data, r.data, data_len);

    free(r.data);
    krs_fragment_result_destroy(&split);
    krs_reassembler_destroy(&reassembler);
    free(data);
}

void test_fragment_reassemble_out_of_order(void) {
    uint32_t data_len = (uint32_t)KRS_MAX_PAYLOAD_PER_FRAGMENT * 2 + 50;
    uint8_t* data = malloc(data_len);
    for (uint32_t i = 0; i < data_len; i++) data[i] = (uint8_t)((i * 3 + 7) & 0xFF);

    FragmentResult_t split = krs_fragment_split(2, BASIC_MESSAGE, 33, data, data_len, KRS_DEFAULT_MTU, 0);
    TEST_ASSERT_TRUE(split.base.valid);
    TEST_ASSERT_EQUAL_UINT16(3, split.fragment_count);

    Reassembler_t* reassembler = krs_reassembler_create();

    uint8_t b0[KRS_DEFAULT_MTU], b1[KRS_DEFAULT_MTU], b2[KRS_DEFAULT_MTU];

    Frame_t f2 = krs_frame_create(split.fragments[2], split.fragment_sizes[2], b2, sizeof(b2));
    ReassembleResult_t r2 = krs_reassembler_feed(reassembler, &f2);
    TEST_ASSERT_FALSE(r2.complete);
    TEST_ASSERT_NULL(r2.data);

    Frame_t f0 = krs_frame_create(split.fragments[0], split.fragment_sizes[0], b0, sizeof(b0));
    ReassembleResult_t r0 = krs_reassembler_feed(reassembler, &f0);
    TEST_ASSERT_FALSE(r0.complete);
    TEST_ASSERT_NULL(r0.data);

    Frame_t f1 = krs_frame_create(split.fragments[1], split.fragment_sizes[1], b1, sizeof(b1));
    ReassembleResult_t r1 = krs_reassembler_feed(reassembler, &f1);
    TEST_ASSERT_TRUE(r1.complete);
    TEST_ASSERT_NOT_NULL(r1.data);
    TEST_ASSERT_EQUAL_UINT32(data_len, r1.data_length);
    TEST_ASSERT_EQUAL_MEMORY(data, r1.data, data_len);

    free(r1.data);
    krs_fragment_result_destroy(&split);
    krs_reassembler_destroy(&reassembler);
    free(data);
}

void test_fragment_reassemble_single_frame(void) {
    uint8_t data[50];
    memset(data, 0xCC, sizeof(data));

    FragmentResult_t split = krs_fragment_split(3, BASIC_MESSAGE, 11, data, sizeof(data), KRS_DEFAULT_MTU, 0);
    TEST_ASSERT_EQUAL_UINT16(1, split.fragment_count);

    Reassembler_t* reassembler = krs_reassembler_create();

    uint8_t body_buf[200];
    Frame_t frame = krs_frame_create(split.fragments[0], split.fragment_sizes[0], body_buf, sizeof(body_buf));
    ReassembleResult_t r = krs_reassembler_feed(reassembler, &frame);

    TEST_ASSERT_TRUE(r.complete);
    TEST_ASSERT_NOT_NULL(r.data);
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), r.data_length);
    TEST_ASSERT_EQUAL_MEMORY(data, r.data, sizeof(data));

    free(r.data);
    krs_fragment_result_destroy(&split);
    krs_reassembler_destroy(&reassembler);
}

void test_fragment_split_invalid_mtu(void) {
    uint8_t data[10];
    FragmentResult_t result = krs_fragment_split(0, BASIC_MESSAGE, 1, data, sizeof(data), 5, 0);
    TEST_ASSERT_FALSE(result.base.valid);
    TEST_ASSERT_EQUAL_UINT16(0, result.fragment_count);
    TEST_ASSERT_NULL(result.fragments);
}

void test_fragment_split_wire_format(void) {
    uint8_t data[] = {0x11, 0x22, 0x33};

    FragmentResult_t result = krs_fragment_split(7, BASIC_MESSAGE, 0x0102030405060708ULL,
                                                  data, sizeof(data), KRS_DEFAULT_MTU, 0);
    TEST_ASSERT_TRUE(result.base.valid);
    TEST_ASSERT_EQUAL_UINT16(1, result.fragment_count);

    uint8_t* f = result.fragments[0];
    TEST_ASSERT_EQUAL_HEX8(0x4B, f[0]);
    TEST_ASSERT_EQUAL_UINT8(7, f[2]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)BASIC_MESSAGE, f[3]);
    TEST_ASSERT_EQUAL_HEX16(0, ((uint16_t)f[4] << 8) | f[5]);
    uint64_t pid = 0;
    for (int k = 0; k < 8; k++) pid = (pid << 8) | f[6 + k];
    TEST_ASSERT_EQUAL_UINT64(0x0102030405060708ULL, pid);
    TEST_ASSERT_EQUAL_MEMORY(data, f + KRONOS_FRAME_HEADER_LENGTH, sizeof(data));

    krs_fragment_result_destroy(&result);
}

void test_fragment_split_malloc_failure(void) {
    uint8_t data[100];
    mock_malloc_fail_next();
    FragmentResult_t result = krs_fragment_split(1, BASIC_MESSAGE, 42, data, sizeof(data), KRS_DEFAULT_MTU, 0);
    TEST_ASSERT_FALSE(result.base.valid);
    TEST_ASSERT_NULL(result.fragments);
    TEST_ASSERT_NULL(result.fragment_sizes);
}

void test_reassembler_create_malloc_failure(void) {
    mock_malloc_fail_next();
    Reassembler_t* r = krs_reassembler_create();
    TEST_ASSERT_NULL(r);
}

void test_reassembler_destroy_null(void) {
    krs_reassembler_destroy(NULL);

    Reassembler_t* r = NULL;
    krs_reassembler_destroy(&r);
    TEST_ASSERT_NULL(r);
}

void test_reassembler_feed_null_params(void) {
    Reassembler_t* r = krs_reassembler_create();

    uint8_t body[] = {1, 2, 3};
    Frame_t frame = {0};
    frame.presence_flags = 0;
    frame.body = body;
    frame.body_length = sizeof(body);

    ReassembleResult_t res_null_r = krs_reassembler_feed(NULL, &frame);
    TEST_ASSERT_FALSE(res_null_r.base.valid);

    ReassembleResult_t res_null_f = krs_reassembler_feed(r, NULL);
    TEST_ASSERT_FALSE(res_null_f.base.valid);

    krs_reassembler_destroy(&r);
}

void test_reassembler_feed_single_no_fraginfo(void) {
    uint8_t body[] = {0xAA, 0xBB, 0xCC, 0xDD};

    Frame_t frame = {0};
    frame.protocol_char = 'K';
    frame.packet_id = 100;
    frame.presence_flags = 0;
    frame.body = body;
    frame.body_length = sizeof(body);

    Reassembler_t* r = krs_reassembler_create();
    ReassembleResult_t result = krs_reassembler_feed(r, &frame);

    TEST_ASSERT_TRUE(result.complete);
    TEST_ASSERT_NOT_NULL(result.data);
    TEST_ASSERT_EQUAL_UINT32(sizeof(body), result.data_length);
    TEST_ASSERT_EQUAL_MEMORY(body, result.data, sizeof(body));

    free(result.data);
    krs_reassembler_destroy(&r);
}

void test_reassembler_feed_two_fragments_direct(void) {
    uint8_t p0[] = {0xAA, 0xBB};
    uint8_t p1[] = {0xCC, 0xDD, 0xEE};

    Frame_t f0 = {0};
    f0.packet_id = 200;
    f0.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    f0.metadata.fragment_index = 0;
    f0.metadata.fragment_total = 2;
    f0.body = p0;
    f0.body_length = sizeof(p0);

    Frame_t f1 = {0};
    f1.packet_id = 200;
    f1.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    f1.metadata.fragment_index = 1;
    f1.metadata.fragment_total = 2;
    f1.body = p1;
    f1.body_length = sizeof(p1);

    Reassembler_t* r = krs_reassembler_create();

    ReassembleResult_t r0 = krs_reassembler_feed(r, &f0);
    TEST_ASSERT_FALSE(r0.complete);
    TEST_ASSERT_NULL(r0.data);

    ReassembleResult_t r1 = krs_reassembler_feed(r, &f1);
    TEST_ASSERT_TRUE(r1.complete);
    TEST_ASSERT_NOT_NULL(r1.data);

    uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    TEST_ASSERT_EQUAL_UINT32(sizeof(expected), r1.data_length);
    TEST_ASSERT_EQUAL_MEMORY(expected, r1.data, sizeof(expected));

    free(r1.data);
    krs_reassembler_destroy(&r);
}

void test_reassembler_feed_out_of_order_direct(void) {
    uint8_t p0[] = {1, 2};
    uint8_t p1[] = {3, 4};
    uint8_t p2[] = {5, 6, 7};

    Frame_t f0 = {0}; f0.packet_id = 300; f0.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO); f0.metadata.fragment_index = 0; f0.metadata.fragment_total = 3; f0.body = p0; f0.body_length = sizeof(p0);
    Frame_t f1 = {0}; f1.packet_id = 300; f1.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO); f1.metadata.fragment_index = 1; f1.metadata.fragment_total = 3; f1.body = p1; f1.body_length = sizeof(p1);
    Frame_t f2 = {0}; f2.packet_id = 300; f2.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO); f2.metadata.fragment_index = 2; f2.metadata.fragment_total = 3; f2.body = p2; f2.body_length = sizeof(p2);

    Reassembler_t* r = krs_reassembler_create();

    ReassembleResult_t res2 = krs_reassembler_feed(r, &f2);
    TEST_ASSERT_FALSE(res2.complete);

    ReassembleResult_t res0 = krs_reassembler_feed(r, &f0);
    TEST_ASSERT_FALSE(res0.complete);

    ReassembleResult_t res1 = krs_reassembler_feed(r, &f1);
    TEST_ASSERT_TRUE(res1.complete);

    uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7};
    TEST_ASSERT_EQUAL_UINT32(sizeof(expected), res1.data_length);
    TEST_ASSERT_EQUAL_MEMORY(expected, res1.data, sizeof(expected));

    free(res1.data);
    krs_reassembler_destroy(&r);
}

void test_reassembler_feed_duplicate_fragment(void) {
    uint8_t p0[] = {0x01, 0x02, 0x03};

    Frame_t f0 = {0};
    f0.packet_id = 400;
    f0.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    f0.metadata.fragment_index = 0;
    f0.metadata.fragment_total = 2;
    f0.body = p0;
    f0.body_length = sizeof(p0);

    Reassembler_t* r = krs_reassembler_create();

    ReassembleResult_t first = krs_reassembler_feed(r, &f0);
    TEST_ASSERT_FALSE(first.complete);

    ReassembleResult_t dup = krs_reassembler_feed(r, &f0);
    TEST_ASSERT_FALSE(dup.complete);

    FragmentSession_t* session = KRS_ARRAY_GET(r->sessions, 0, FragmentSession_t);
    TEST_ASSERT_NOT_NULL(session);
    TEST_ASSERT_EQUAL_UINT16(1, session->received);

    krs_reassembler_destroy(&r);
}

void test_reassembler_feed_oversized_payload_rejected(void) {
    uint8_t body[KRS_MAX_PAYLOAD_PER_FRAGMENT + 1];
    memset(body, 0xAB, sizeof(body));

    Frame_t f = {0};
    f.packet_id = 500;
    f.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    f.metadata.fragment_index = 0;
    f.metadata.fragment_total = 2;
    f.body = body;
    f.body_length = sizeof(body);

    Reassembler_t* r = krs_reassembler_create();
    TEST_ASSERT_NOT_NULL(r);

    ReassembleResult_t result = krs_reassembler_feed(r, &f);
    TEST_ASSERT_FALSE(result.base.valid);
    TEST_ASSERT_FALSE(result.complete);
    TEST_ASSERT_NULL(result.data);

    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(r->sessions));

    krs_reassembler_destroy(&r);
}

void test_reassembler_feed_exact_max_payload_accepted(void) {
    uint8_t body[KRS_MAX_PAYLOAD_PER_FRAGMENT];
    memset(body, 0x7E, sizeof(body));

    Frame_t f = {0};
    f.packet_id = 501;
    f.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    f.metadata.fragment_index = 0;
    f.metadata.fragment_total = 2;
    f.body = body;
    f.body_length = sizeof(body);

    Reassembler_t* r = krs_reassembler_create();
    TEST_ASSERT_NOT_NULL(r);

    ReassembleResult_t result = krs_reassembler_feed(r, &f);
    TEST_ASSERT_TRUE(result.base.valid);
    TEST_ASSERT_FALSE(result.complete);

    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(r->sessions));

    krs_reassembler_destroy(&r);
}

void test_reassembler_rejects_oversized_total(void) {
    Reassembler_t* r = krs_reassembler_create();
    TEST_ASSERT_NOT_NULL(r);

    Frame_t frame = {0};
    frame.protocol_char = 0x4B;
    frame.channel = 10;
    frame.frame_type = BASIC_MESSAGE;
    frame.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    frame.packet_id = 12345;
    frame.metadata.fragment_index = 0;
    frame.metadata.fragment_total = (uint16_t)(KRS_MAX_FRAGMENTS_PER_PACKET + 1);
    frame.body_length = 0;
    frame.body = NULL;

    ReassembleResult_t result = krs_reassembler_feed(r, &frame);
    TEST_ASSERT_FALSE(result.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_INVALID_PARAMETER, result.base.error_code);

    krs_reassembler_destroy(&r);
}

void test_reassembler_rejects_when_session_cap_reached(void) {
    Reassembler_t* r = krs_reassembler_create();
    TEST_ASSERT_NOT_NULL(r);

    uint8_t body[12];
    for (int i = 0; i < 12; i++) body[i] = 0xCC;

    Frame_t frame = {0};
    frame.protocol_char = 0x4B;
    frame.channel = 10;
    frame.frame_type = BASIC_MESSAGE;
    frame.presence_flags = (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
    frame.metadata.fragment_index = 0;
    frame.metadata.fragment_total = 8;
    frame.body_length = 12;
    frame.body = body;

    for (uint32_t i = 0; i < KRS_MAX_REASSEMBLY_SESSIONS; i++) {
        frame.packet_id = 1000 + i;
        ReassembleResult_t res = krs_reassembler_feed(r, &frame);
        TEST_ASSERT_TRUE(res.base.valid);
        TEST_ASSERT_FALSE(res.complete);
    }

    frame.packet_id = 999999;
    ReassembleResult_t over = krs_reassembler_feed(r, &frame);
    TEST_ASSERT_FALSE(over.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_MEMORY_ALLOCATION, over.base.error_code);

    krs_reassembler_destroy(&r);
}
