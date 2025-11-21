#include <unity.h>

void setUp() {}

void tearDown() {}

void test_port_table_internal_prime_sizes_exist(void);
void test_port_table_creation(void);


void main() {
    UNITY_BEGIN();

    RUN_TEST(test_port_table_internal_prime_sizes_exist);
    RUN_TEST(test_port_table_creation);

    UNITY_END();
}