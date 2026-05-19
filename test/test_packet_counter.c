#include "kronos_packet_counter.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdint.h>
#include <windows.h>


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

typedef struct CounterStressArgs {
    PacketCounter_t* counter;
    Channel_t channel;
    uint32_t increments;
} CounterStressArgs_t;

static DWORD WINAPI s_counter_stress_thread(LPVOID param) {
    CounterStressArgs_t* args = (CounterStressArgs_t*)param;
    for (uint32_t i = 0; i < args->increments; i++) {
        (void)krs_packet_counter_next(args->counter, args->channel);
    }
    return 0;
}

void test_packet_counter_concurrent_next_no_lost_increments(void) {
    PacketCounter_t* counter = krs_packet_counter_create();
    TEST_ASSERT_NOT_NULL(counter);

    enum { THREAD_COUNT = 8, INCREMENTS_PER_THREAD = 100000 };
    HANDLE threads[THREAD_COUNT];
    CounterStressArgs_t args = {
        .counter = counter,
        .channel = 10,
        .increments = INCREMENTS_PER_THREAD
    };

    for (int i = 0; i < THREAD_COUNT; i++) {
        threads[i] = CreateThread(NULL, 0, s_counter_stress_thread, &args, 0, NULL);
        TEST_ASSERT_NOT_NULL(threads[i]);
    }
    WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(threads[i]);

    uint64_t expected = (uint64_t)THREAD_COUNT * INCREMENTS_PER_THREAD;
    TEST_ASSERT_EQUAL_UINT64(expected, krs_packet_counter_current(counter, 10));

    for (int ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
        if (ch == 10) continue;
        TEST_ASSERT_EQUAL_UINT64(0u, krs_packet_counter_current(counter, (Channel_t)ch));
    }

    krs_packet_counter_destroy(&counter);
    TEST_ASSERT_NULL(counter);
}
