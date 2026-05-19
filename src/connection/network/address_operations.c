#include "kronos_network.h"

#include <string.h>

#include <winsock2.h>


bool krs_network_port_address_equals(const PortAddress_t* a, const PortAddress_t* b) {
    if (!a || !b) return false;
    if (a->sin6_family != b->sin6_family) return false;
    if (a->sin6_port != b->sin6_port) return false;
    return memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(struct in6_addr)) == 0;
}
