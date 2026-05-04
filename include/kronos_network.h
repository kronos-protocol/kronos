#ifndef KRONOS_NETWORK_H
#define KRONOS_NETWORK_H
#include "kronos_error.h"
#include "kronos_platform.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/** @brief Platform socket handle wrapping the underlying UDP socket. */
typedef KrsSocketHandle_t UDPSocketRef_t;

/** @brief Opaque endpoint combining an address, socket, and channel. */
typedef struct Endpoint Endpoint_t;

/** @brief 8-bit channel identifier (0–255). */
typedef uint8_t Channel_t;

/** @brief IPv6 socket address used for all network operations. */
typedef KrsNetworkAddress_t PortAddress_t;

/** @brief IPv6 address (wraps in6_addr). IPv4 addresses are stored as IPv4-mapped IPv6. */
typedef struct in6_addr Address_t;

/** @brief Result type for address creation functions. */
typedef struct AddressCreateResult AddressCreate_r;

/** @brief 16-bit UDP port number. */
typedef uint16_t Port_t;

/** @brief Associates a socket handle with its port number. */
typedef struct UDPSocketRefPort UDPSocketRefPort_t;

/** @brief Channel-type label (see enum ChannelType for current semantics). */
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
 * @brief Application-visible channel-type label.
 *
 * @warning This enum carries NO protocol semantics. The server passes the
 *          per-channel label verbatim to application callbacks via
 *          `ChannelMessageCallback_f`. The label is set only by the
 *          (functionally inert) `SOCKET_SETUP` frame handler. Applications
 *          MUST NOT rely on this value or branch on it for protocol
 *          decisions. See Known Issue #17 in SPEC.md.
 *
 * The server treats every channel uniformly with respect to connection
 * tracking, broadcasting, send-by-ID, and lifecycle callbacks. The value is
 * initialised to OPEN_CHANNEL, may be flipped to SOCKET_CHANNEL by a
 * SOCKET_SETUP frame (purely as a relabel — see frame type 12), and is
 * forwarded as-is via the `channel_type` parameter of ChannelMessageCallback_f.
 *
 * OPEN_CHANNEL    — Default value assigned when a port is initialised.
 * MESSAGE_CHANNEL — Reserved label, currently unused by the protocol.
 * SOCKET_CHANNEL  — Set by SOCKET_SETUP frames; carries no extra semantics.
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
 * @return Bound socket handle, or KRS_INVALID_SOCKET on failure.
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
