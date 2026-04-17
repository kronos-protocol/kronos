#include "kronos_network.h"
#include "network_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <winsock2.h>


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

