#include "malloc_wrapper.h"
#include "unity.h"
#include "mock_kronos_server.h"
#include "server_internal.h"
#include "mock_kronos_port_table.h"


#include <kronos_port_table.h>
#include <port_table_internal.h>

static UdpSocketHandler_t udp_socket_handler;

void test_port_table_internal_prime_sizes_exist(void) {
    TEST_ASSERT_TRUE(PRIME_SIZES_COUNT > 0);
}

void test_port_table_creation(void) {
    mock_malloc_fail_next();
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTable_t* pt_empty = krs_lib_port_table_create();
    TEST_ASSERT_NULL(pt_empty);

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);

    TEST_ASSERT_EQUAL_UINT32(PRIME_SIZES[0], pt->table_size);
    TEST_ASSERT_EQUAL_UINT32(0, pt->total_entries);
    TEST_ASSERT_EQUAL_UINT8(0, pt->prime_size_index);

    for (uint32_t i = 0; i < pt->table_size; i++) {
        TEST_ASSERT_NULL(pt->table[i]);
    }

    krs_lib_port_table_destroy(&pt);
}

void test_port_table_insert(void) {
    krs_server_udp_socket_handler_destroy_Ignore();

    PortAddress_t dummy_port_address = {0};
    PortLink_t* port_link_arr[1] = {NULL};
    PortLink_t** port_linkpp = port_link_arr;
    PortTable_t port_table = {.table = port_linkpp, .table_size = 1, .prime_size_index = -1, .total_entries = 0};
    PortTable_t* pt = &port_table;

    Port_t port = 10;
    uint32_t expected_index = HASH_PORT_INDEX(port, pt->table_size);

    TEST_ASSERT_NULL(pt->table[expected_index]);

    krs_lib_port_table_insert(pt, port, krs_server_udp_socket_handler_create(dummy_port_address));

    TEST_ASSERT_NOT_NULL(pt->table[expected_index]);
    TEST_ASSERT_EQUAL_UINT32(1, pt->total_entries);
    TEST_ASSERT_EQUAL(pt->table[expected_index]->port, port);
    TEST_ASSERT_NULL(pt->table[expected_index]->next);

    krs_lib_port_table_insert(pt, port, krs_server_udp_socket_handler_create(dummy_port_address));
    TEST_ASSERT_EQUAL_UINT32(1, pt->total_entries);

    krs_lib_port_table_destroy(&pt);
}

void test_port_table_destroy(void) {
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);

    krs_lib_port_table_destroy(&pt);
    TEST_ASSERT_NULL(pt);
}