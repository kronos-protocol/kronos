#include "malloc_wrapper.h"
#include "unity.h"
#include "mock_kronos_server.h"


#include <kronos_port_table.h>
#include <port_table_internal.h>

static void mock_socket_handler_destroy() {
    krs_server_socket_handler_destroy_Ignore();
}


void test_port_table_internal_prime_sizes_exist(void) {
    TEST_ASSERT_TRUE(PRIME_SIZES_COUNT > 0);
}

void test_port_table_creation(void) {
    mock_malloc_fail_next();
    mock_socket_handler_destroy();

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
    mock_socket_handler_destroy();

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);

    Port_t port = 10;
    uint32_t expected_index = HASH_PORT_INDEX(port, pt->table_size);

    TEST_ASSERT_NULL(pt->table[expected_index]);

    krs_lib_port_table_insert(pt, port);

    TEST_ASSERT_NOT_NULL(pt->table[expected_index]);
    TEST_ASSERT_EQUAL_UINT32(1, pt->total_entries);
    TEST_ASSERT_EQUAL(pt->table[expected_index]->port, port);
    TEST_ASSERT_NULL(pt->table[expected_index]->next);

    krs_lib_port_table_insert(pt, port);
    TEST_ASSERT_EQUAL_UINT32(1, pt->total_entries);

    krs_lib_port_table_destroy(&pt);
}

void test_port_table_destroy(void) {
    mock_socket_handler_destroy();

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);

    krs_lib_port_table_destroy(&pt);
    TEST_ASSERT_NULL(pt);
}