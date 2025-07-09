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

#define MAX_CHANNEL_NUMBER 255;
#define MAX_PORT_NUMBER 65535;

#endif //KRONOS_NETWORK_H
