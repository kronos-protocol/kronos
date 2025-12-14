#include "malloc_wrapper.h"
#include "unity.h"
#include "mock_kronos_server.h"
#include "server_internal.h"
#include "port_table_internal.h"
#include <kronos_port_table.h>


static void test_port_table_fields_empty(PortTable_t* pt) {
    TEST_ASSERT_NOT_NULL_MESSAGE(pt, "PortTable pointer is NULL");
    TEST_ASSERT_NOT_NULL_MESSAGE(pt->table, "PortTable->table is NULL");

    size_t expected_table_size = PRIME_SIZES[0];
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected_table_size, pt->table_size,
        "PortTable->table_size does not match PRIME_SIZES[0]");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, pt->total_entries, "PortTable->total_entries is not 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, pt->prime_size_index, "PortTable->prime_size_index is not 0");

    for (uint32_t i = 0; i < pt->table_size; i++) {
        TEST_ASSERT_NULL_MESSAGE(pt->table[i], "Expected table slot to be NULL");
    }
}

static void test_port_table_field_null(PortTable_t* pt, uint32_t index, int depth) {
    TEST_ASSERT_NOT_NULL_MESSAGE(pt, "PortTable pointer is NULL");
    TEST_ASSERT_NOT_NULL_MESSAGE(pt->table, "PortTable->table is NULL");

    PortLink_t* pl = pt->table[index];
    for (int i = 1; i <= depth; i++) {
        if (pl->next != NULL) {
            pl = pl->next;
        }
    }

    TEST_ASSERT_NULL_MESSAGE(pl, "Expected PortLink at depth to be NULL");
}

static void test_port_table_field(PortTable_t* pt, uint32_t index, int depth, Port_t port, bool has_next) {
    TEST_ASSERT_NOT_NULL_MESSAGE(pt, "PortTable pointer is NULL");
    TEST_ASSERT_NOT_NULL_MESSAGE(pt->table, "PortTable->table is NULL");
    TEST_ASSERT_NOT_NULL_MESSAGE(pt->table[index], "PortTable->table[index] is NULL for depth traversal");

    PortLink_t* pl = pt->table[index];
    for (int i = 1; i <= depth; i++) {
        pl = pl->next;
    }

    TEST_ASSERT_NOT_NULL_MESSAGE(pl, "PortLink at depth is NULL");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(port, pl->port, "PortLink->port does not match expected value");
    TEST_ASSERT_NOT_NULL_MESSAGE(pl->socket_handler, "PortLink->socket_handler is NULL");
    TEST_ASSERT_NULL_MESSAGE(pl->next, "PortLink->next should be NULL");
}

void test_port_table_internal_prime_sizes_exist(void) {
    TEST_ASSERT_TRUE_MESSAGE(PRIME_SIZES_COUNT > 0, "PRIME_SIZES_COUNT is 0");
}

void test_port_table_creation(void) {
    mock_malloc_fail_next();
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTable_t* pt_empty = krs_lib_port_table_create();
    TEST_ASSERT_NULL_MESSAGE(pt_empty, "Expected NULL for malloc-failed PortTable");

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL_MESSAGE(pt, "PortTable creation failed");
    test_port_table_fields_empty(pt);

    krs_lib_port_table_destroy(&pt);
}

void test_port_table_creation_s(void) {
    mock_malloc_fail_next();
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTableCreate_r pt_empty = krs_lib_port_table_create_s();
    TEST_ASSERT_FALSE_MESSAGE(pt_empty.base.valid, "Expected invalid result due to malloc failure");
    TEST_ASSERT_NULL_MESSAGE(pt_empty.port_table, "Expected NULL PortTable pointer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(KRS_ERR_MEMORY_ALLOCATION, pt_empty.base.error_code,
        "Error code does not match KRS_ERR_MEMORY_ALLOCATION");

    PortTableCreate_r pt = krs_lib_port_table_create_s();
    TEST_ASSERT_NOT_NULL_MESSAGE(pt.port_table, "PortTable creation failed");
    TEST_ASSERT_TRUE_MESSAGE(pt.base.valid, "Result should be valid");
    TEST_ASSERT_EQUAL_INT_MESSAGE(KRS_ERR_SUCCESS, pt.base.error_code, "Error code does not match KRS_ERR_SUCCESS");
    TEST_ASSERT_FALSE_MESSAGE(pt.base.free_error_message, "free_error_message should be false");
    test_port_table_fields_empty(pt.port_table);

    krs_lib_port_table_destroy(&pt.port_table);
}

void test_port_table_insert(void) {
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL_MESSAGE(pt, "PortTable creation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(pt->table, "PortTable->table is NULL");

    UdpSocketHandler_t udp_socket_handler_fake_struct;
    UdpSocketHandler_t* udp_socket_handler_fake = &udp_socket_handler_fake_struct;

    Port_t port = 10;
    Port_t port2 = 11;
    uint32_t expected_index = hash_port_index(port, pt->table_size);

    TEST_ASSERT_NULL_MESSAGE(pt->table[expected_index], "Expected first table slot to be NULL");

    krs_lib_port_table_insert(pt, port, NULL);
    test_port_table_field_null(pt, 0, 0);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, pt->total_entries, "total_entries should remain 0 after NULL insert");

    krs_lib_port_table_insert(pt, port, udp_socket_handler_fake);
    test_port_table_field(pt, 4, 0, port, false); // ALERT: This test breaks if the hash_port_index function is changed
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, pt->total_entries, "total_entries should be 1 after first real insert");

    krs_lib_port_table_insert(pt, port, udp_socket_handler_fake);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, pt->total_entries, "total_entries should remain 1 after duplicate insert");

    krs_lib_port_table_insert(pt, port2, udp_socket_handler_fake);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, pt->total_entries, "total_entries should be 2 after second port insert");

    krs_lib_port_table_destroy(&pt);
}

void test_port_table_destroy(void) {
    krs_server_udp_socket_handler_destroy_Ignore();

    PortTable_t* pt = krs_lib_port_table_create();
    TEST_ASSERT_NOT_NULL_MESSAGE(pt, "PortTable creation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(pt->table, "PortTable->table is NULL");

    krs_lib_port_table_destroy(&pt);
    TEST_ASSERT_NULL_MESSAGE(pt, "PortTable pointer should be NULL after destroy");
}
