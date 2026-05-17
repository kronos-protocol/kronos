#include "kronos_server.h"
#include "message_pool_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>


void test_message_pool_basic_acquire_release(void) {
    MessagePool_t* pool = krs_message_pool_create(8);
    TEST_ASSERT_NOT_NULL(pool);

    IncomingMessage_t* msgs[8];
    for (int i = 0; i < 8; i++) {
        msgs[i] = krs_message_pool_acquire(pool);
        TEST_ASSERT_NOT_NULL(msgs[i]);
    }
    for (int i = 0; i < 8; i++) {
        krs_message_pool_release(pool, msgs[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(8, pool->free_count);

    krs_message_pool_destroy(&pool);
}

void test_message_pool_fallback_within_cap_succeeds(void) {
    MessagePool_t* pool = krs_message_pool_create(4);

    IncomingMessage_t* pooled[4];
    for (int i = 0; i < 4; i++) pooled[i] = krs_message_pool_acquire(pool);

    IncomingMessage_t* fallback[100];
    for (int i = 0; i < 100; i++) {
        fallback[i] = krs_message_pool_acquire(pool);
        TEST_ASSERT_NOT_NULL_MESSAGE(fallback[i], "fallback within cap must succeed");
    }
    TEST_ASSERT_EQUAL_INT64(100, pool->outstanding_fallback_count);

    for (int i = 0; i < 100; i++) krs_message_pool_release(pool, fallback[i]);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, pool->outstanding_fallback_count,
        "outstanding count must return to zero after all fallbacks released");

    for (int i = 0; i < 4; i++) krs_message_pool_release(pool, pooled[i]);
    krs_message_pool_destroy(&pool);
}

void test_message_pool_fallback_cap_returns_null(void) {
    MessagePool_t* pool = krs_message_pool_create(2);

    IncomingMessage_t* pooled[2];
    pooled[0] = krs_message_pool_acquire(pool);
    pooled[1] = krs_message_pool_acquire(pool);

    enum { CAP = 1024 };
    IncomingMessage_t** fallback = malloc(CAP * sizeof(IncomingMessage_t*));
    TEST_ASSERT_NOT_NULL(fallback);
    for (int i = 0; i < CAP; i++) {
        fallback[i] = krs_message_pool_acquire(pool);
        TEST_ASSERT_NOT_NULL(fallback[i]);
    }

    IncomingMessage_t* over = krs_message_pool_acquire(pool);
    TEST_ASSERT_NULL_MESSAGE(over, "acquire past cap must return NULL");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(CAP, pool->outstanding_fallback_count,
        "outstanding count must NOT exceed the cap (rejected acquire was rolled back)");

    krs_message_pool_release(pool, fallback[0]);
    fallback[0] = krs_message_pool_acquire(pool);
    TEST_ASSERT_NOT_NULL_MESSAGE(fallback[0], "acquire after release should succeed again");

    for (int i = 0; i < CAP; i++) krs_message_pool_release(pool, fallback[i]);
    free(fallback);
    krs_message_pool_release(pool, pooled[0]);
    krs_message_pool_release(pool, pooled[1]);
    krs_message_pool_destroy(&pool);
}

void test_message_pool_fallback_count_lifetime_monotonic(void) {
    MessagePool_t* pool = krs_message_pool_create(2);
    IncomingMessage_t* a = krs_message_pool_acquire(pool);
    IncomingMessage_t* b = krs_message_pool_acquire(pool);
    IncomingMessage_t* c = krs_message_pool_acquire(pool);
    IncomingMessage_t* d = krs_message_pool_acquire(pool);

    TEST_ASSERT_EQUAL_UINT64(2, krs_message_pool_get_fallback_count(pool));

    krs_message_pool_release(pool, c);
    krs_message_pool_release(pool, d);

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(2, krs_message_pool_get_fallback_count(pool),
        "lifetime count must NOT decrement on release");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, pool->outstanding_fallback_count,
        "outstanding count MUST decrement on release");

    krs_message_pool_release(pool, a);
    krs_message_pool_release(pool, b);
    krs_message_pool_destroy(&pool);
}

typedef struct PoolStressArgs {
    MessagePool_t* pool;
    uint32_t iterations;
} PoolStressArgs_t;

static DWORD WINAPI s_pool_stress_thread(LPVOID param) {
    PoolStressArgs_t* args = (PoolStressArgs_t*)param;
    for (uint32_t i = 0; i < args->iterations; i++) {
        IncomingMessage_t* msg = krs_message_pool_acquire(args->pool);
        if (msg) {
            krs_message_pool_release(args->pool, msg);
        }
    }
    return 0;
}

void test_message_pool_concurrent_acquire_release_outstanding_returns_zero(void) {
    MessagePool_t* pool = krs_message_pool_create(8);

    enum { THREADS = 8, ITERATIONS = 5000 };
    HANDLE threads[THREADS];
    PoolStressArgs_t args = { .pool = pool, .iterations = ITERATIONS };

    for (int i = 0; i < THREADS; i++) {
        threads[i] = CreateThread(NULL, 0, s_pool_stress_thread, &args, 0, NULL);
        TEST_ASSERT_NOT_NULL(threads[i]);
    }
    WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE);
    for (int i = 0; i < THREADS; i++) CloseHandle(threads[i]);

    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, pool->outstanding_fallback_count,
        "after all threads finish, outstanding must be zero");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(8, pool->free_count,
        "after all threads finish, pool must be fully replenished");

    krs_message_pool_destroy(&pool);
}
