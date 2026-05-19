#include "kronos_ack.h"
#include "ack_internal.h"
#include "kronos_array.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdint.h>
#include <windows.h>


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

    krs_ack_tracker_expect(tracker, 42, 0, data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT64(42, entry->packet_id);
    TEST_ASSERT_EQUAL_UINT16(sizeof(data), entry->frame_size);
    TEST_ASSERT_EQUAL_UINT8(0, entry->retry_count);

    krs_ack_tracker_receive(tracker, 42, 0);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_receive_unknown_id(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0xAA};

    krs_ack_tracker_expect(tracker, 10, 0, data, sizeof(data));
    krs_ack_tracker_receive(tracker, 99, 0);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_timeout_retry(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0xBB};

    krs_ack_tracker_expect(tracker, 99, 0, data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;

    uint64_t retry_ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);

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

    krs_ack_tracker_expect(tracker, 7, 0, data, sizeof(data));

    uint64_t ids[4];
    uint32_t count;

    AckEntry_t* entry;

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_multiple_concurrent(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t d1[] = {1};
    uint8_t d2[] = {2};
    uint8_t d3[] = {3};

    krs_ack_tracker_expect(tracker, 1, 0, d1, sizeof(d1));
    krs_ack_tracker_expect(tracker, 2, 0, d2, sizeof(d2));
    krs_ack_tracker_expect(tracker, 3, 0, d3, sizeof(d3));
    TEST_ASSERT_EQUAL_UINT32(3, krs_array_length(tracker->pending));

    krs_ack_tracker_receive(tracker, 2, 0);
    TEST_ASSERT_EQUAL_UINT32(2, krs_array_length(tracker->pending));

    AckEntry_t* e0 = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    AckEntry_t* e1 = KRS_ARRAY_GET(tracker->pending, 1, AckEntry_t);
    e0->timestamp_ms = 0;
    e1->timestamp_ms = 0;

    uint64_t ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(2, count);

    krs_ack_tracker_receive(tracker, ids[0], 0);
    krs_ack_tracker_receive(tracker, ids[1], 0);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_no_timeout_when_fresh(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 3);
    uint8_t data[] = {0xDD};

    krs_ack_tracker_expect(tracker, 55, 0, data, sizeof(data));

    uint64_t ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
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
    krs_ack_tracker_expect(tracker, 42, 0, data, sizeof(data));

    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_expect_null_tracker(void) {
    uint8_t data[] = {1};
    krs_ack_tracker_expect(NULL, 42, 0, data, sizeof(data));
}

void test_ack_tracker_receive_null_tracker(void) {
    krs_ack_tracker_receive(NULL, 42, 0);
}

void test_ack_tracker_check_timeouts_null_params(void) {
    TEST_ASSERT_EQUAL_UINT32(0, krs_ack_tracker_check_timeouts(NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL));

    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    TEST_ASSERT_EQUAL_UINT32(0, krs_ack_tracker_check_timeouts(tracker, NULL, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL));
    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_frame_data_copied(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    krs_ack_tracker_expect(tracker, 111, 0, data, sizeof(data));

    data[0] = 0xFF;

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_EQUAL_UINT8(0xDE, entry->frame_data[0]);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_check_timeouts_capacity_limit(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 3);
    uint8_t d[] = {1};

    krs_ack_tracker_expect(tracker, 10, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 20, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 30, 0, d, sizeof(d));

    for (uint32_t i = 0; i < 3; i++) {
        AckEntry_t* e = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        e->timestamp_ms = 0;
    }

    uint64_t ids[2];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 2, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(3, count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_receive_removes_all_entries_with_same_id(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(5000, 3);
    TEST_ASSERT_NOT_NULL(tracker);

    uint8_t data1[] = {0xAA};
    uint8_t data2[] = {0xBB};
    uint8_t data3[] = {0xCC};
    krs_ack_tracker_expect(tracker, 42, 0, data1, sizeof(data1));
    krs_ack_tracker_expect(tracker, 42, 0, data2, sizeof(data2));
    krs_ack_tracker_expect(tracker, 42, 0, data3, sizeof(data3));
    krs_ack_tracker_expect(tracker, 99, 0, data1, sizeof(data1));

    krs_ack_tracker_receive(tracker, 42, 0);

    uint64_t retry_ids[8];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 8, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(0, count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_reports_dropped_ids(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 2);
    uint8_t data[] = {0xEE};

    krs_ack_tracker_expect(tracker, 555, 0, data, sizeof(data));

    uint64_t ids[4];
    uint64_t dropped[4];
    uint32_t dropped_count = 0;
    uint32_t count;

    AckEntry_t* entry;
    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, dropped, NULL, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(0, dropped_count);

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, dropped, NULL, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(0, dropped_count);

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, dropped, NULL, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(1, dropped_count);
    TEST_ASSERT_EQUAL_UINT64(555, dropped[0]);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_dropped_null_out_is_safe(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 1);
    uint8_t data[] = {0x11};

    krs_ack_tracker_expect(tracker, 777, 0, data, sizeof(data));

    uint64_t ids[4];
    uint32_t count;

    AckEntry_t* entry;
    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, count);

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    entry->timestamp_ms = 0;
    count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_dropped_capacity_limit(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 0);
    uint8_t d[] = {1};

    krs_ack_tracker_expect(tracker, 1, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 2, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 3, 0, d, sizeof(d));

    for (uint32_t i = 0; i < 3; i++) {
        AckEntry_t* e = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        e->timestamp_ms = 0;
    }

    uint64_t ids[4];
    uint64_t dropped[2];
    uint32_t dropped_count = 0;
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, ids, NULL, NULL, NULL, 4, dropped, NULL, 2, &dropped_count);

    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(2, dropped_count);
    TEST_ASSERT_EQUAL_UINT32(0, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_default_enabled(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 5);
    TEST_ASSERT_TRUE(krs_ack_tracker_is_fast_retransmit_enabled(tracker));
    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_can_disable(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 5);
    krs_ack_tracker_set_fast_retransmit_enabled(tracker, false);
    TEST_ASSERT_FALSE(krs_ack_tracker_is_fast_retransmit_enabled(tracker));
    krs_ack_tracker_set_fast_retransmit_enabled(tracker, true);
    TEST_ASSERT_TRUE(krs_ack_tracker_is_fast_retransmit_enabled(tracker));
    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_triggers_on_three_later_acks(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 100, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 101, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 102, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 103, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 101, 0);
    krs_ack_tracker_receive(tracker, 102, 0);
    krs_ack_tracker_receive(tracker, 103, 0);

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT64(100, entry->packet_id);
    TEST_ASSERT_EQUAL_UINT8(3, entry->acked_after_count);

    uint64_t retry_ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);

    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT64(100, retry_ids[0]);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(0, entry->acked_after_count);
    TEST_ASSERT_EQUAL_UINT8(1, entry->retry_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_does_not_trigger_below_threshold(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 200, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 201, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 202, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 201, 0);
    krs_ack_tracker_receive(tracker, 202, 0);

    uint64_t retry_ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);

    TEST_ASSERT_EQUAL_UINT32(0, count);

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(2, entry->acked_after_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_disabled_does_not_trigger(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    krs_ack_tracker_set_fast_retransmit_enabled(tracker, false);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 300, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 301, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 302, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 303, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 301, 0);
    krs_ack_tracker_receive(tracker, 302, 0);
    krs_ack_tracker_receive(tracker, 303, 0);

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(0, entry->acked_after_count);

    uint64_t retry_ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4, NULL, NULL, 0, NULL);

    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_then_normal_timeout_drops(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 1);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 400, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 401, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 402, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 403, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 401, 0);
    krs_ack_tracker_receive(tracker, 402, 0);
    krs_ack_tracker_receive(tracker, 403, 0);

    uint64_t retry_ids[4];
    uint64_t dropped_ids[4];
    uint32_t dropped_count = 0;
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4,
                                                     dropped_ids, NULL, 4, &dropped_count);

    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT64(400, retry_ids[0]);
    TEST_ASSERT_EQUAL_UINT32(0, dropped_count);

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    entry->timestamp_ms = 0;

    count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4,
                                            dropped_ids, NULL, 4, &dropped_count);

    TEST_ASSERT_EQUAL_UINT32(0, count);
    TEST_ASSERT_EQUAL_UINT32(1, dropped_count);
    TEST_ASSERT_EQUAL_UINT64(400, dropped_ids[0]);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_does_not_count_earlier_acks(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 500, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 499, 0);
    krs_ack_tracker_receive(tracker, 498, 0);
    krs_ack_tracker_receive(tracker, 497, 0);

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(0, entry->acked_after_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_channel_preserved_on_retry(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(50, 3);
    uint8_t data[] = {0x01};

    krs_ack_tracker_expect(tracker, 100, 17, data, sizeof(data));

    Sleep(80);

    uint64_t retry_ids[4];
    uint8_t  retry_channels[4];
    uint32_t n = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_channels, NULL, NULL, 4,
                                                NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, n);
    TEST_ASSERT_EQUAL_UINT64(100, retry_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(17, retry_channels[0]);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_channel_preserved_on_drop(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(50, 0);
    uint8_t data[] = {0x02};

    krs_ack_tracker_expect(tracker, 200, 42, data, sizeof(data));

    Sleep(80);

    uint64_t retry_ids[4];
    uint8_t  retry_channels[4];
    uint64_t dropped_ids[4];
    uint8_t  dropped_channels[4];
    uint32_t dropped_n = 0;
    uint32_t retry_n = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_channels, NULL, NULL, 4,
                                                       dropped_ids, dropped_channels, 4, &dropped_n);

    TEST_ASSERT_EQUAL_UINT32(0, retry_n);
    TEST_ASSERT_EQUAL_UINT32(1, dropped_n);
    TEST_ASSERT_EQUAL_UINT64(200, dropped_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(42, dropped_channels[0]);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_channel_per_entry_independent(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(50, 3);
    uint8_t data[] = {0x03};

    krs_ack_tracker_expect(tracker, 1, 10, data, sizeof(data));
    krs_ack_tracker_expect(tracker, 2, 20, data, sizeof(data));
    krs_ack_tracker_expect(tracker, 3, 30, data, sizeof(data));

    Sleep(80);

    uint64_t retry_ids[8];
    uint8_t  retry_channels[8];
    uint32_t n = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_channels, NULL, NULL, 8,
                                                NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(3, n);

    bool found_10 = false, found_20 = false, found_30 = false;
    for (uint32_t i = 0; i < 3; i++) {
        if (retry_ids[i] == 1) { TEST_ASSERT_EQUAL_UINT8(10, retry_channels[i]); found_10 = true; }
        if (retry_ids[i] == 2) { TEST_ASSERT_EQUAL_UINT8(20, retry_channels[i]); found_20 = true; }
        if (retry_ids[i] == 3) { TEST_ASSERT_EQUAL_UINT8(30, retry_channels[i]); found_30 = true; }
    }
    TEST_ASSERT_TRUE(found_10);
    TEST_ASSERT_TRUE(found_20);
    TEST_ASSERT_TRUE(found_30);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_check_timeouts_returns_valid_entry_handles(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(50, 5);
    uint8_t fake[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    krs_ack_tracker_expect(tracker, 100, 5, fake, sizeof(fake));

    Sleep(80);

    uint64_t ids[4];
    uint8_t  chans[4];
    AckEntry_t* entries[4];
    uint64_t drops[4];
    uint8_t  drop_chans[4];
    uint32_t drop_count = 0;
    uint32_t n = krs_ack_tracker_check_timeouts(tracker, ids, chans, entries, NULL, 4,
                                                drops, drop_chans, 4, &drop_count);
    TEST_ASSERT_EQUAL_UINT32(1, n);
    TEST_ASSERT_EQUAL_UINT64(100, ids[0]);
    TEST_ASSERT_EQUAL_UINT8(5, chans[0]);
    TEST_ASSERT_NOT_NULL(entries[0]);

    uint16_t sz = 0;
    const uint8_t* data = krs_ack_tracker_get_retry_frame_for_entry(entries[0], &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT16(8, sz);
    TEST_ASSERT_EQUAL_MEMORY(fake, data, 8);

    TEST_ASSERT_NULL(krs_ack_tracker_get_retry_frame_for_entry(NULL, &sz));
    TEST_ASSERT_EQUAL_UINT16(0, sz);

    krs_ack_tracker_destroy(&tracker);
}

static AckEntry_t* s_first_entry(AckTracker_t* tracker) {
    return KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
}

void test_ack_tracker_retry_uses_exponential_backoff(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 5);
    krs_ack_tracker_set_fast_retransmit_enabled(tracker, false);
    TEST_ASSERT_NOT_NULL(tracker);

    uint8_t frame[] = {0xAB, 0xCD};
    krs_ack_tracker_expect(tracker, 100, 10, frame, sizeof(frame));

    AckEntry_t* entry = s_first_entry(tracker);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT8(0, entry->retry_count);

    uint64_t retry_ids[4]; uint8_t retry_chans[4]; AckEntry_t* retry_entries[4];
    uint64_t dropped_ids[4]; uint8_t dropped_chans[4]; uint32_t dropped_count = 0;

    entry->timestamp_ms -= 1100;
    uint32_t r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                                 dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r, "1100ms with retry=0 must trigger");
    TEST_ASSERT_EQUAL_UINT8(1, entry->retry_count);

    entry->timestamp_ms -= 1100;
    r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                       dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, r,
        "with retry=1, 1100ms must NOT trigger - backoff is 2000ms");
    TEST_ASSERT_EQUAL_UINT8(1, entry->retry_count);

    entry->timestamp_ms -= 1000;
    r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                       dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r, "2100ms with retry=1 must trigger (2x backoff = 2000ms)");
    TEST_ASSERT_EQUAL_UINT8(2, entry->retry_count);

    entry->timestamp_ms -= 3000;
    r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                       dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, r,
        "with retry=2, 3000ms must NOT trigger - backoff is 4000ms");

    entry->timestamp_ms -= 1500;
    r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                       dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r, "4500ms with retry=2 must trigger (4x backoff = 4000ms)");
    TEST_ASSERT_EQUAL_UINT8(3, entry->retry_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_backoff_clamps_at_60_seconds(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(20000, 5);
    krs_ack_tracker_set_fast_retransmit_enabled(tracker, false);

    uint8_t frame[] = {0x01};
    krs_ack_tracker_expect(tracker, 200, 10, frame, sizeof(frame));
    AckEntry_t* entry = s_first_entry(tracker);

    entry->retry_count = 2;

    uint64_t retry_ids[4]; uint8_t retry_chans[4]; AckEntry_t* retry_entries[4];
    uint64_t dropped_ids[4]; uint8_t dropped_chans[4]; uint32_t dropped_count = 0;

    entry->timestamp_ms -= 60001;
    uint32_t r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                                 dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r, "60001ms with clamped 60000ms backoff must trigger");

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_unaffected_by_backoff(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(1000, 5);

    uint8_t frame[] = {0xFF};
    krs_ack_tracker_expect(tracker, 300, 10, frame, sizeof(frame));
    AckEntry_t* entry = s_first_entry(tracker);

    entry->retry_count = 3;
    entry->acked_after_count = 3;

    uint64_t retry_ids[4]; uint8_t retry_chans[4]; AckEntry_t* retry_entries[4];
    uint64_t dropped_ids[4]; uint8_t dropped_chans[4]; uint32_t dropped_count = 0;
    uint32_t r = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries, NULL, 4,
                                                 dropped_ids, dropped_chans, 4, &dropped_count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r,
        "fast-retransmit must fire even when timeout backoff is far away");
    TEST_ASSERT_EQUAL_UINT8(4, entry->retry_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_does_not_leak_across_channels(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 1, 10, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 100, 42);
    krs_ack_tracker_receive(tracker, 101, 42);
    krs_ack_tracker_receive(tracker, 102, 42);

    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT64(1, entry->packet_id);
    TEST_ASSERT_EQUAL_UINT8(10, entry->channel);
    TEST_ASSERT_EQUAL_UINT8(0, entry->acked_after_count);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_fast_retransmit_only_counts_same_channel(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 1, 10, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 1, 42, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 2, 10, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 2, 42, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 3, 10, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 4, 10, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 2, 10);
    krs_ack_tracker_receive(tracker, 3, 10);
    krs_ack_tracker_receive(tracker, 4, 10);

    uint32_t len = krs_array_length(tracker->pending);
    for (uint32_t i = 0; i < len; i++) {
        AckEntry_t* e = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        if (!e) continue;
        if (e->channel == 10 && e->packet_id == 1) {
            TEST_ASSERT_EQUAL_UINT8(3, e->acked_after_count);
        } else if (e->channel == 42 && e->packet_id == 1) {
            TEST_ASSERT_EQUAL_UINT8(0, e->acked_after_count);
        } else if (e->channel == 42 && e->packet_id == 2) {
            TEST_ASSERT_EQUAL_UINT8(0, e->acked_after_count);
        }
    }

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_tracker_receive_does_not_remove_other_channel_same_pid(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 7, 10, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 7, 42, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 7, 10);

    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(tracker->pending));
    AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, 0, AckEntry_t);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT64(7, entry->packet_id);
    TEST_ASSERT_EQUAL_UINT8(42, entry->channel);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_check_timeouts_reports_fast_vs_timeout(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 100, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 101, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 102, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 103, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 101, 0);
    krs_ack_tracker_receive(tracker, 102, 0);
    krs_ack_tracker_receive(tracker, 103, 0);

    krs_ack_tracker_expect(tracker, 200, 1, d, sizeof(d));
    AckEntry_t* timed_out = KRS_ARRAY_GET(tracker->pending, krs_array_length(tracker->pending) - 1, AckEntry_t);
    TEST_ASSERT_NOT_NULL(timed_out);
    TEST_ASSERT_EQUAL_UINT64(200, timed_out->packet_id);
    timed_out->timestamp_ms = 0;

    uint64_t retry_ids[8];
    uint8_t  retry_chans[8];
    AckEntry_t* retry_entries[8];
    bool     retry_was_fast[8];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, retry_chans, retry_entries,
                                                    retry_was_fast, 8, NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(2, count);

    bool found_fr = false;
    bool found_to = false;
    for (uint32_t i = 0; i < count; i++) {
        if (retry_ids[i] == 100) {
            TEST_ASSERT_TRUE_MESSAGE(retry_was_fast[i],
                "packet 100 had 3 later ACKs on channel 0 -> must be fast-retransmit");
            TEST_ASSERT_EQUAL_UINT8(0, retry_chans[i]);
            found_fr = true;
        } else if (retry_ids[i] == 200) {
            TEST_ASSERT_FALSE_MESSAGE(retry_was_fast[i],
                "packet 200 had no later ACKs on its channel -> must be timeout");
            TEST_ASSERT_EQUAL_UINT8(1, retry_chans[i]);
            found_to = true;
        }
    }
    TEST_ASSERT_TRUE(found_fr);
    TEST_ASSERT_TRUE(found_to);

    krs_ack_tracker_destroy(&tracker);
}

void test_ack_check_timeouts_was_fast_optional(void) {
    AckTracker_t* tracker = krs_ack_tracker_create(60000, 5);
    uint8_t d[] = {0xAB};

    krs_ack_tracker_expect(tracker, 100, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 101, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 102, 0, d, sizeof(d));
    krs_ack_tracker_expect(tracker, 103, 0, d, sizeof(d));

    krs_ack_tracker_receive(tracker, 101, 0);
    krs_ack_tracker_receive(tracker, 102, 0);
    krs_ack_tracker_receive(tracker, 103, 0);

    uint64_t retry_ids[4];
    uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, NULL, NULL, NULL, 4,
                                                    NULL, NULL, 0, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT64(100, retry_ids[0]);

    krs_ack_tracker_destroy(&tracker);
}
