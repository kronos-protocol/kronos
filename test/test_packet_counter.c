#include "kronos_packet_counter.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdint.h>


void test_packet_counter_create_destroy(void) {
    PacketCounter_t* counter = krs_packet_counter_create();
    TEST_ASSERT_NOT_NULL(counter);
    krs_packet_counter_destroy(&counter);
    TEST_ASSERT_NULL(counter);
}

void test_packet_counter_starts_at_zero(void) {
    PacketCounter_t* counter = krs_packet_counter_create();

    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 0));
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 1));
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 255));

    krs_packet_counter_destroy(&counter);
}

void test_packet_counter_next_increments(void) {
    PacketCounter_t* counter = krs_packet_counter_create();

    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_next(counter, 0));
    TEST_ASSERT_EQUAL_UINT64(2, krs_packet_counter_next(counter, 0));
    TEST_ASSERT_EQUAL_UINT64(3, krs_packet_counter_next(counter, 0));
    TEST_ASSERT_EQUAL_UINT64(3, krs_packet_counter_current(counter, 0));

    krs_packet_counter_destroy(&counter);
}

void test_packet_counter_per_channel_independence(void) {
    PacketCounter_t* counter = krs_packet_counter_create();

    krs_packet_counter_next(counter, 1);
    krs_packet_counter_next(counter, 1);
    krs_packet_counter_next(counter, 2);

    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 0));
    TEST_ASSERT_EQUAL_UINT64(2, krs_packet_counter_current(counter, 1));
    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_current(counter, 2));
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 255));

    krs_packet_counter_destroy(&counter);
}

void test_packet_counter_reset(void) {
    PacketCounter_t* counter = krs_packet_counter_create();

    krs_packet_counter_next(counter, 5);
    krs_packet_counter_next(counter, 5);
    TEST_ASSERT_EQUAL_UINT64(2, krs_packet_counter_current(counter, 5));

    krs_packet_counter_reset(counter, 5);
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 5));
    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_next(counter, 5));

    krs_packet_counter_destroy(&counter);
}

void test_packet_counter_reset_isolated(void) {
    PacketCounter_t* counter = krs_packet_counter_create();

    krs_packet_counter_next(counter, 10);
    krs_packet_counter_next(counter, 11);
    krs_packet_counter_next(counter, 11);

    krs_packet_counter_reset(counter, 10);

    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 10));
    TEST_ASSERT_EQUAL_UINT64(2, krs_packet_counter_current(counter, 11));

    krs_packet_counter_destroy(&counter);
}

void test_packet_counter_null_safety(void) {
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_next(NULL, 0));
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(NULL, 0));
    krs_packet_counter_reset(NULL, 0);
    krs_packet_counter_destroy(NULL);

    PacketCounter_t* counter = NULL;
    krs_packet_counter_destroy(&counter);
    TEST_ASSERT_NULL(counter);
}

void test_packet_counter_create_malloc_failure(void) {
    mock_malloc_fail_next();
    PacketCounter_t* counter = krs_packet_counter_create();
    TEST_ASSERT_NULL(counter);
}

void test_packet_counter_boundary_channels(void) {
    PacketCounter_t* counter = krs_packet_counter_create();

    krs_packet_counter_next(counter, 0);
    krs_packet_counter_next(counter, 127);
    krs_packet_counter_next(counter, 128);
    krs_packet_counter_next(counter, 255);

    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_current(counter, 0));
    TEST_ASSERT_EQUAL_UINT64(0, krs_packet_counter_current(counter, 1));
    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_current(counter, 127));
    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_current(counter, 128));
    TEST_ASSERT_EQUAL_UINT64(1, krs_packet_counter_current(counter, 255));

    krs_packet_counter_destroy(&counter);
}
