#ifndef KRONOS_NETWORK_H
#define KRONOS_NETWORK_H
#include <stdint.h>
#include <winsock2.h>

typedef SOCKET UDPSocketRef_t;
typedef struct Endpoint Endpoint_t;
typedef uint8_t Channel_t;
typedef struct Connection Connection_t;
typedef struct sockaddr_in6 Address_t;

enum ChannelType {
    MESSAGE_CHANNEL,
    SOCKET_CHANNEL
};

#endif //KRONOS_NETWORK_H
