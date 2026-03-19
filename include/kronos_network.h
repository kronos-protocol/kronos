#ifndef KRONOS_NETWORK_H
#define KRONOS_NETWORK_H
#include "kronos_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

/** @brief SOCKET handle wrapping the platform UDP socket. */
typedef SOCKET UDPSocketRef_t;

/** @brief Opaque endpoint combining an address, socket, and channel. */
typedef struct Endpoint Endpoint_t;

/** @brief 8-bit channel identifier (0–255). */
typedef uint8_t Channel_t;

/** @brief Opaque connection object. */
typedef struct Connection Connection_t;

/** @brief IPv6 socket address used for all network operations (wraps sockaddr_in6). */
typedef struct sockaddr_in6 PortAddress_t;

/** @brief IPv6 address (wraps in6_addr). IPv4 addresses are stored as IPv4-mapped IPv6. */
typedef struct in6_addr Address_t;

/** @brief Result type for address creation functions. */
typedef struct AddressCreateResult AddressCreate_r;

/** @brief 16-bit UDP port number. */
typedef uint16_t Port_t;

/** @brief Associates a socket handle with its port number. */
typedef struct UDPSocketRefPort UDPSocketRefPort_t;

/** @brief Channel operating mode. */
typedef enum ChannelType ChannelType_e;

struct UDPSocketRefPort {
    UDPSocketRef_t socket;
    int port;
};

struct AddressCreateResult {
    KronosResult_b base;
    Address_t address;
};

/**
 * @brief Channel operating mode — controls how the server handles messages on a channel.
 *
 * OPEN_CHANNEL    — No connection tracking; messages are processed and discarded.
 * MESSAGE_CHANNEL — Request/response datagram mode with optional ACK.
 * SOCKET_CHANNEL  — WebSocket-like persistent connection with per-client tracking.
 */
enum ChannelType {
    OPEN_CHANNEL,
    MESSAGE_CHANNEL,
    SOCKET_CHANNEL
};

/**
 * @brief Creates an IPv4 address from a null-terminated string.
 *
 * @param ip  Null-terminated IPv4 string (e.g. "192.168.1.1").
 * @return Address_t with the mapped IPv6 representation, or zero on error.
 */
Address_t krs_network_address_ipv4_create(const char* ip);

/**
 * @brief Creates an IPv4 address from a null-terminated string with explicit error handling.
 *
 * @param ip  Null-terminated IPv4 string.
 * @return AddressCreate_r containing the address or error information.
 *
 * @retval KRS_SUCCESS                 Address created successfully.
 * @retval KRS_ERR_NULL_POINTER        ip is NULL.
 * @retval KRS_ERR_TOO_LONG            ip exceeds MAX_IPV4_LENGTH.
 * @retval KRS_ERR_NETWORK_INVALID_IP  ip is not a valid IPv4 address.
 */
AddressCreate_r krs_network_address_ipv4_create_s(const char* ip);

/**
 * @brief Creates an IPv4 address from a string with a known length.
 *
 * @param ip         IPv4 string (must be null-terminated within ip_length bytes).
 * @param ip_length  Length of the string including the null terminator.
 * @return Address_t with the mapped IPv6 representation, or zero on error.
 */
Address_t krs_network_address_ipv4_create_len(const char* ip, uint8_t ip_length);

/**
 * @brief Creates an IPv4 address from a string with a known length, with explicit error handling.
 *
 * @param ip         IPv4 string (must be null-terminated within ip_length bytes).
 * @param ip_length  Length of the string including the null terminator.
 * @return AddressCreate_r containing the address or error information.
 *
 * @retval KRS_SUCCESS                   Address created successfully.
 * @retval KRS_ERR_NULL_POINTER          ip is NULL.
 * @retval KRS_ERR_TOO_LONG              ip_length exceeds MAX_IPV4_LENGTH.
 * @retval KRS_ERR_NOT_NULL_TERMINATED   String is not null-terminated within ip_length.
 * @retval KRS_ERR_NETWORK_INVALID_IP    ip is not a valid IPv4 address.
 */
AddressCreate_r krs_network_address_ipv4_create_len_s(const char* ip, uint8_t ip_length);

/**
 * @brief Creates an IPv6 address from a string with a known length.
 *
 * @param ip         IPv6 string (must be null-terminated within ip_length bytes).
 * @param ip_length  Length of the string including the null terminator.
 * @return Address_t, or zero on error.
 */
Address_t krs_network_address_ipv6_create(char* ip, uint8_t ip_length);

/**
 * @brief Creates a PortAddress_t (sockaddr_in6) from a port number and an Address_t.
 *
 * @param port     UDP port number.
 * @param address  IPv6 address (or IPv4-mapped IPv6 address).
 * @return PortAddress_t ready for bind/connect.
 */
PortAddress_t krs_network_port_address_create(Port_t port, Address_t address);

/**
 * @brief Creates a bound UDP socket for the given port address.
 *
 * @param port_address  The address and port to bind to.
 * @return Bound SOCKET handle, or INVALID_SOCKET on failure.
 */
UDPSocketRef_t krs_network_udp_socket_ref_create(PortAddress_t port_address);

/**
 * @brief Compares two PortAddress_t for equality (family, port, address).
 *
 * Ignores padding bytes in sockaddr_in6 that could cause false negatives with memcmp.
 *
 * @param a  First address to compare.
 * @param b  Second address to compare.
 * @return true if both addresses represent the same endpoint, false otherwise.
 */
bool krs_network_port_address_equals(const PortAddress_t* a, const PortAddress_t* b);

/** @brief Maximum channel number (channels are 0–255). */
#define MAX_CHANNEL_NUMBER 255

/** @brief Maximum valid port number. */
#define MAX_PORT_NUMBER 65535

/** @brief Maximum length of an IPv6 address string including null terminator. */
#define MAX_IPV6_LENGTH 46

/** @brief Maximum length of an IPv4 address string including null terminator. */
#define MAX_IPV4_LENGTH 16

#endif // KRONOS_NETWORK_H
