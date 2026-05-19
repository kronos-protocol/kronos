#include "kronos_network.h"
#include "network_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <string.h>


void test_addr_eq_same_endpoint(void) {
    krs_wsa_init();

    Address_t addr = krs_network_address_ipv4_create("192.168.1.1");
    PortAddress_t a = krs_network_port_address_create(8080, addr);
    PortAddress_t b = krs_network_port_address_create(8080, addr);

    TEST_ASSERT_TRUE(krs_network_port_address_equals(&a, &b));

    krs_wsa_cleanup();
}

void test_addr_eq_different_port(void) {
    krs_wsa_init();

    Address_t addr = krs_network_address_ipv4_create("192.168.1.1");
    PortAddress_t a = krs_network_port_address_create(8080, addr);
    PortAddress_t b = krs_network_port_address_create(9090, addr);

    TEST_ASSERT_FALSE(krs_network_port_address_equals(&a, &b));

    krs_wsa_cleanup();
}

void test_addr_eq_different_address(void) {
    krs_wsa_init();

    Address_t addr_a = krs_network_address_ipv4_create("192.168.1.1");
    Address_t addr_b = krs_network_address_ipv4_create("192.168.1.2");
    PortAddress_t a = krs_network_port_address_create(8080, addr_a);
    PortAddress_t b = krs_network_port_address_create(8080, addr_b);

    TEST_ASSERT_FALSE(krs_network_port_address_equals(&a, &b));

    krs_wsa_cleanup();
}

void test_addr_eq_null_params(void) {
    PortAddress_t addr;
    memset(&addr, 0, sizeof(addr));

    TEST_ASSERT_FALSE(krs_network_port_address_equals(NULL, &addr));
    TEST_ASSERT_FALSE(krs_network_port_address_equals(&addr, NULL));
    TEST_ASSERT_FALSE(krs_network_port_address_equals(NULL, NULL));
}

void test_addr_eq_zeroed_padding(void) {
    krs_wsa_init();

    Address_t ip = krs_network_address_ipv4_create("10.0.0.1");

    PortAddress_t addr_a;
    memset(&addr_a, 0, sizeof(addr_a));
    addr_a.sin6_family = AF_INET6;
    addr_a.sin6_port = htons(5000);
    addr_a.sin6_addr = ip;

    PortAddress_t addr_b;
    memset(&addr_b, 0xCC, sizeof(addr_b));
    addr_b.sin6_family = AF_INET6;
    addr_b.sin6_port = htons(5000);
    addr_b.sin6_addr = ip;

    TEST_ASSERT_TRUE(krs_network_port_address_equals(&addr_a, &addr_b));

    krs_wsa_cleanup();
}

void test_addr_eq_ipv4_mapped(void) {
    krs_wsa_init();

    Address_t addr1 = krs_network_address_ipv4_create("127.0.0.1");
    Address_t addr2 = krs_network_address_ipv4_create("127.0.0.1");
    PortAddress_t a = krs_network_port_address_create(7777, addr1);
    PortAddress_t b = krs_network_port_address_create(7777, addr2);

    TEST_ASSERT_TRUE(krs_network_port_address_equals(&a, &b));

    krs_wsa_cleanup();
}
