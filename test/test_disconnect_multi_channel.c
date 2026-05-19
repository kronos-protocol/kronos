#include "unity.h"
#include "kronos.h"
#include "kronos_server.h"
#include "kronos_client.h"
#include "kronos_network.h"
#include "malloc_wrapper.h"

#include <windows.h>

void test_disconnect_with_three_subscriptions_releases_once(void) {
    mock_malloc_reset();

    int malloc_before = mock_malloc_get_call_count();
    int calloc_before = mock_calloc_get_call_count();
    int free_before = mock_free_get_call_count();

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    krs_server_port_manager_port_add(spm, 19503);
    krs_server_start(spm);

    PortAddress_t srv = krs_network_port_address_create(19503, addr);
    ServerConnection_t* conn = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(conn);
    krs_client_start_receive(conn);

    TEST_ASSERT_TRUE(krs_client_subscribe(conn, 10, 2000).base.valid);
    TEST_ASSERT_TRUE(krs_client_subscribe(conn, 20, 2000).base.valid);
    TEST_ASSERT_TRUE(krs_client_subscribe(conn, 30, 2000).base.valid);

    krs_client_disconnect(&conn);
    Sleep(300);

    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);

    int malloc_after = mock_malloc_get_call_count();
    int calloc_after = mock_calloc_get_call_count();
    int free_after = mock_free_get_call_count();
    int net_allocs = ((malloc_after - malloc_before) + (calloc_after - calloc_before))
                     - (free_after - free_before);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, net_allocs,
        "alloc/free imbalance after full subscribe-disconnect lifecycle. "
        "Negative = double-free (refcount underflow). "
        "Positive = leak. Should be exactly zero.");
}
