#include "kronos_network.h"
#include "network_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>


void test_wsa_init_cleanup_single(void) {
    krs_wsa_init();

    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(INVALID_SOCKET, s);
    closesocket(s);

    krs_wsa_cleanup();
}

void test_wsa_init_double_cleanup_double(void) {
    krs_wsa_init();
    krs_wsa_init();

    krs_wsa_cleanup();

    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(INVALID_SOCKET, s);
    closesocket(s);

    krs_wsa_cleanup();
}

void test_wsa_cleanup_without_init(void) {
    krs_wsa_cleanup();

    krs_wsa_init();
    krs_wsa_init();

    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(INVALID_SOCKET, s);
    closesocket(s);

    krs_wsa_cleanup();
}

typedef struct WsaStressArgs {
    uint32_t iterations;
} WsaStressArgs_t;

static DWORD WINAPI s_wsa_init_cleanup_thread(LPVOID param) {
    WsaStressArgs_t* args = (WsaStressArgs_t*)param;
    for (uint32_t i = 0; i < args->iterations; i++) {
        krs_wsa_init();
        SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (s != INVALID_SOCKET) closesocket(s);
        krs_wsa_cleanup();
    }
    return 0;
}

void test_wsa_init_cleanup_concurrent_no_crash(void) {
    enum { THREAD_COUNT = 16, ITERATIONS_PER_THREAD = 1000 };
    HANDLE threads[THREAD_COUNT];
    WsaStressArgs_t args = { .iterations = ITERATIONS_PER_THREAD };

    for (int i = 0; i < THREAD_COUNT; i++) {
        threads[i] = CreateThread(NULL, 0, s_wsa_init_cleanup_thread, &args, 0, NULL);
        TEST_ASSERT_NOT_NULL(threads[i]);
    }
    WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; i++) CloseHandle(threads[i]);

    krs_wsa_init();
    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(INVALID_SOCKET, s);
    closesocket(s);
    krs_wsa_cleanup();
}

