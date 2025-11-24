#include "malloc_wrapper.h"


#include <unity.h>

void setUp() {
    mock_malloc_reset();
}

void tearDown() {
    mock_malloc_reset();
}

void test_port_table_internal_prime_sizes_exist(void);
void test_port_table_creation(void);
void test_port_table_insert(void);
void test_port_table_destroy(void);


void main() {
    UNITY_BEGIN();

    RUN_TEST(test_port_table_internal_prime_sizes_exist);
    RUN_TEST(test_port_table_creation);
    RUN_TEST(test_port_table_insert);
    RUN_TEST(test_port_table_destroy);

    UNITY_END();
}