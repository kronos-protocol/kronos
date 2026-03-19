#include "kronos_network.h"
#include "network_internal.h"

#include <string.h>


Address_t krs_network_address_ipv4_create(const char* ip) {
    if (!ip) return (Address_t){0};
    size_t len = strnlen(ip, MAX_IPV4_LENGTH);
    if (len >= MAX_IPV4_LENGTH) return (Address_t){0};
    return krs_network_address_ipv4_create_len(ip, (uint8_t)(len + 1));
}

AddressCreate_r krs_network_address_ipv4_create_s(const char* ip) {
    AddressCreate_r result = {0};
    if (!ip) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "ip is NULL");
        return result;
    }
    size_t len = strnlen(ip, MAX_IPV4_LENGTH);
    if (len >= MAX_IPV4_LENGTH) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_TOO_LONG, "ip exceeds MAX_IPV4_LENGTH");
        return result;
    }
    return krs_network_address_ipv4_create_len_s(ip, (uint8_t)(len + 1));
}

Address_t krs_network_address_ipv4_create_len(const char* ip, uint8_t ip_length) {
    if (!ip || ip_length > MAX_IPV4_LENGTH) return (Address_t){0};
    if (ip[ip_length - 1] != '\0') return (Address_t){0};

    struct in_addr ipv4_addr;
    if (inet_pton(AF_INET, ip, &ipv4_addr) != 1) return (Address_t){0};

    Address_t result = {0};
    result.s6_addr[10] = 0xFF;
    result.s6_addr[11] = 0xFF;
    memcpy(&result.s6_addr[12], &ipv4_addr, 4);
    return result;
}

AddressCreate_r krs_network_address_ipv4_create_len_s(const char* ip, uint8_t ip_length) {
    AddressCreate_r result = {0};
    if (!ip) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "ip is NULL");
        return result;
    }
    if (ip_length > MAX_IPV4_LENGTH) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_TOO_LONG, "ip_length exceeds MAX_IPV4_LENGTH");
        return result;
    }
    if (ip[ip_length - 1] != '\0') {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_NULL_TERMINATED, "ip is not null-terminated within ip_length");
        return result;
    }

    struct in_addr ipv4_addr;
    if (inet_pton(AF_INET, ip, &ipv4_addr) != 1) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NETWORK_INVALID_IP, "ip is not a valid IPv4 address");
        return result;
    }

    Address_t address = {0};
    address.s6_addr[10] = 0xFF;
    address.s6_addr[11] = 0xFF;
    memcpy(&address.s6_addr[12], &ipv4_addr, 4);

    result.base = krs_lib_error_result_base_suc();
    result.address = address;
    return result;
}

Address_t krs_network_address_ipv6_create(char* ip, uint8_t ip_length) {
    if (!ip || ip_length > MAX_IPV6_LENGTH) return (Address_t){0};
    if (ip[ip_length - 1] != '\0') return (Address_t){0};

    Address_t result;
    if (inet_pton(AF_INET6, ip, &result) != 1) return (Address_t){0};
    return result;
}

PortAddress_t krs_network_port_address_create(Port_t port, Address_t address) {
    PortAddress_t port_address = {0};
    port_address.sin6_family = AF_INET6;
    port_address.sin6_port = htons(port);
    port_address.sin6_addr = address;
    return port_address;
}
