#include "kronos_ack.h"
#include "ack_internal.h"
#include "kronos_array.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdint.h>


void test_ack_tracker_create_destroy(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    TEST_ASSERT_NOT_NULL(tracker);
    TEST_ASSERT_NOT_NULL(tracker->pending);
    TEST_ASSERT_EQUAL_UINT32(1000, tracker->timeout_ms);
    TEST_ASSERT_EQUAL_UINT8(3, tracker->max_retries);

    krs_ack_tracker_destroy(&tracker);
    TEST_ASSERT_NULL(tracker);
}

void test_ack_tracker_destroy_null(void) {
    AckTracker_t* tracker = NULL;
    krs_ack_tracker_destroy(&tracker);
    TEST_ASSERT_NULL(tracker);

    krs_ack_tracker_destroy(NULL);
}

void test_ack_tracker_expect_and_receive(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0x01, 0x02, 0x03};

    krs_ack_tracker_expect(tracker, 42, data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT64(42, entry->packet_id);
    TEST_ASSERT_EQUAL_UINT16(sizeof(data), entry->frame_size);
    TEST_ASSERT_EQUAL_UINT8(0, entry->retry_count);

    krs_ack_tracker_receive(tracker, 42);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_receive_unknown_id(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0xAA};

    krs_ack_tracker_expect(tracker, 10, data, sizeof(data));
    krs_ack_tracker_receive(tracker, 99);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_timeout_retry(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0xBB};

    krs_ack_tracker_expect(tracker, 99, data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;

    uint64_t retry_ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, 4);

    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT64(99, retry_ids[0]);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_EQUAL_UINT8(1, entry->retry_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_max_retries_drop(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 2);
    uint8_t data[] = {0xCC};

    krs_ack_tracker_expect(tracker, 7, data, sizeof(data));

    uint64_t ids[4];
    uint32_t count;

    AckEntry_t* entry;

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, 4);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, 4);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, 4);
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_multiple_concurrent(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t d1[] = {1};
    uint8_t d2[] = {2};
    uint8_t d3[] = {3};

    krs_ack_tracker_expect(tracker, 1, d1, sizeof(d1));
    krs_ack_tracker_expect(tracker, 2, d2, sizeof(d2));
    krs_ack_tracker_expect(tracker, 3, d3, sizeof(d3));
    TEST_ASSERT_EQUAL_UINT32(3, krs_array_length(tracker->pending));

    krs_ack_tracker_receive(tracker, 2);
    TEST_ASSERT_EQUAL_UINT32(2, krs_array_length(tracker->pending));

    AckEntry_t* e0 = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    AckEntry_t* e1 = KRS_ARRAY_GET(tracker->pending, 1, AckEntry_t);
    e0->timestamp_ms = 0;
    e1->timestamp_ms = 0;

    uint64_t ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, 4);
    TEST_ASSERT_EQUAL_UINT32(2, count);

    krs_ack_tracker_receive(tracker, ids[0]);
    krs_ack_tracker_receive(tracker, ids[1]);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_no_timeout_when_fresh(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 3);
    uint8_t data[] = {0xDD};

    krs_ack_tracker_expect(tracker, 55, data, sizeof(data));

    uint64_t ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, 4);
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_create_malloc_failure(void) {
    mock_malloc_fail_next();
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    TEST_ASSERT_NULL(tracker);
}

void test_ack_tracker_expect_malloc_failure(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {1, 2, 3};

    mock_malloc_fail_next();
    krs_ack_tracker_expect(tracker, 42, data, sizeof(data));

    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_expect_null_tracker(void) {
    uint8_t data[] = {1};
    krs_ack_tracker_expect(NULL, 42, data, sizeof(data));
}

void test_ack_tracker_receive_null_tracker(void) {
    krs_ack_tracker_receive(NULL, 42);
}

void test_ack_tracker_check_timeouts_null_params(void) {
    TEST_ASSERT_EQUAL_UINT32(0, krs_ack_tracker_check_timeouts(NULL, NULL, 0));

    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    TEST_ASSERT_EQUAL_UINT32(0, krs_ack_tracker_check_timeouts(tracker, NULL, 4));
    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_frame_data_copied(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    krs_ack_tracker_expect(tracker, 111, data, sizeof(data));

    data[0] = 0xFF;

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_EQUAL_UINT8(0xDE, entry->frame_data[0]);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_check_timeouts_capacity_limit(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t d[] = {1};

    krs_ack_tracker_expect(tracker, 10, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 20, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 30, d, sizeof(d));

    for (uint32_t i = 0; i < 3; i++) {
        AckEntry_t* e = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        e->timestamp_ms = 0;
    }

    uint64_t ids[2];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, 2);
    TEST_ASSERT_EQUAL_UINT32(3, count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_receive_removes_all_entries_with_same_id(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(5000, 3);
    TEST_ASSERT_NOT_NULL(tracker);

    uint8_t data1[] = {0xAA};
    uint8_t data2[] = {0xBB};
    uint8_t data3[] = {0xCC};
    krs_ack_tracker_expect(tracker, 42, data1, sizeof(data1));
    krs_ack_tracker_expect(tracker, 42, data2, sizeof(data2));
    krs_ack_tracker_expect(tracker, 42, data3, sizeof(data3));
    krs_ack_tracker_expect(tracker, 99, data1, sizeof(data1));

    krs_ack_tracker_receive(tracker, 42);

    uint64_t retry_ids[8];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, 8);
    TEST_ASSERT_EQUAL_UINT32(0, count);

    krs_ack_tracker_destroy(&tracker);
}
