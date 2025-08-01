#include "kronos_network.h"
#include "network_internal.h"


Address_t krs_network_address_ipv4_create_len(const char* ip, const uint8_t ip_length) {
    if (ip_length > MAX_IPV4_LENGTH) {
        //TODO: error handling
        return (Address_t) {0};
    }

    if (ip[ip_length - 1] != '\0') {
        return (Address_t) {0};
    }

    Address_t result;

    inet_pton(AF_INET6, ip, &result);

}
