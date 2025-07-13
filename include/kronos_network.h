#ifndef KRONOS_NETWORK_H
#define KRONOS_NETWORK_H
#include <stdint.h>
#include <winsock2.h>
#include <ws2ipdef.h>

typedef SOCKET UDPSocketRef_t;
typedef struct Endpoint Endpoint_t;
typedef uint8_t Channel_t;
typedef struct Connection Connection_t;
typedef struct sockaddr_in6 PortAddress_t;
typedef struct in6_addr Address_t;
typedef uint16_t Port_t;

enum ChannelType {
    MESSAGE_CHANNEL,
    SOCKET_CHANNEL
};

Address_t krs_network_address_ipv4_create(const char* ip);
AddressResult_t krs_network_address_ipv4_create_s(const char* ip);

Address_t krs_network_address_ipv6_create(char* ip, uint8_t ip_length);

PortAddress_t krs_network_port_address_create(Port_t port, Address_t address);

#define MAX_CHANNEL_NUMBER 255;
#define MAX_PORT_NUMBER 65535;

#define MAX_IPV6_LENGTH 46;
#define MAX_IPV4_LENGTH 16;

#endif //KRONOS_NETWORK_H
