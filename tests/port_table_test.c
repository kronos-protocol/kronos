#include "unity.h"


#include <kronos_port_table.h>
#include <port_table_internal.h>

void test_port_table_internal_prime_sizes_exist(void) {
    TEST_ASSERT_TRUE(PRIME_SIZES_COUNT > 0);
}

void test_port_table_creation(void) {
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
