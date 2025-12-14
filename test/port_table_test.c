#include "malloc_wrapper.h"
#include "unity.h"
#include "mock_kronos_server.h"
#include "server_internal.h"

#define hash_port_index hash_port_index_mock

#include "port_table_internal.h"
#include <kronos_port_table.h>


int hash_port_index_mock(int port, int size) {
    return 0;
}

static void test_port_table_fields_empty(PortTable_t* pt) {
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);

    size_t expected_table_size = PRIME_SIZES[0];

    for (int i = 0; i < expected_table_size; i++) {
        TEST_ASSERT_NULL(pt->table[i]);
    }

    TEST_ASSERT_EQUAL_UINT32(expected_table_size, pt->table_size);
    TEST_ASSERT_EQUAL_UINT32(0, pt->total_entries);
    TEST_ASSERT_EQUAL_UINT8(0, pt->prime_size_index);

    for (uint32_t i = 0; i < pt->table_size; i++) {
        TEST_ASSERT_NULL(pt->table[i]);
    }
}

static void test_port_table_field_null(PortTable_t* pt, uint32_t index, int depth) {
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);
    TEST_ASSERT_NOT_NULL(pt->table[index]);

    PortLink_t* pl = pt->table[index];

    for (int i = 1; i <= depth; i++) {
        pl = pl->next;
    }

    TEST_ASSERT_NULL(pl);
}

static void test_port_table_field(PortTable_t* pt, uint32_t index, int depth, Port_t port, bool has_next) {
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);
    TEST_ASSERT_NOT_NULL(pt->table[index]);

    PortLink_t* pl = pt->table[index];

    for (int i = 1; i <= depth; i++) {
        pl = pl->next;
    }

    TEST_ASSERT_NOT_NULL(pl);
    TEST_ASSERT_EQUAL_UINT32(port, pl->port);
    TEST_ASSERT_NOT_NULL(pl->socket_handler);
    TEST_ASSERT_NULL(pl->next);
}


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
    test_port_table_fields_empty(pt);

    krs_lib_port_table_destroy(&pt);
}

void test_port_table_creation_s(void) {
    mock_malloc_fail_next();
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTableCreate_r pt_empty = krs_lib_port_table_create_s();
    TEST_ASSERT_FALSE(pt_empty.base.valid);
    TEST_ASSERT_NULL(pt_empty.port_table);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_MEMORY_ALLOCATION, pt_empty.base.error_code);

    PortTableCreate_r pt = krs_lib_port_table_create_s();
    TEST_ASSERT_NOT_NULL(pt.port_table);
    TEST_ASSERT_TRUE(pt.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_SUCCESS, pt.base.error_code);
    TEST_ASSERT_FALSE(pt.base.free_error_message);
    test_port_table_fields_empty(pt.port_table);

    krs_lib_port_table_destroy(&pt.port_table);
}

void test_port_table_insert(void) {
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL(pt);
    TEST_ASSERT_NOT_NULL(pt->table);

    UdpSocketHandler_t udp_socket_handler_fake_struct;
    UdpSocketHandler_t* udp_socket_handler_fake = &udp_socket_handler_fake_struct;

    Port_t port = 10;
    Port_t port2 = 11;
    uint32_t expected_index = hash_port_index(port, pt->table_size);

    TEST_ASSERT_NULL(pt->table[expected_index]);

    krs_lib_port_table_insert(pt, port, NULL);
    test_port_table_field_null(pt, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, pt->total_entries);

    krs_lib_port_table_insert(pt, port, udp_socket_handler_fake);
    test_port_table_field(pt, 0, 0, port, false);

    krs_lib_port_table_insert(pt, port, udp_socket_handler_fake);
    test_port_table_field_null(pt, 0, 1);
    TEST_ASSERT_EQUAL_UINT32(1, pt->total_entries);

    krs_lib_port_table_insert(pt, port2, udp_socket_handler_fake);
    test_port_table_field(pt, 0, 0, port, true);
    test_port_table_field(pt, 0, 1, port2, false);

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