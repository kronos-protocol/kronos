#ifndef KRONOS_NETWORK_H
#define KRONOS_NETWORK_H
#include <stdint.h>

typedef struct Endpoint Endpoint_t;
typedef uint8_t Channel_t;
typedef struct Connection Connection_t;

enum ChannelType {
    MESSAGE_CHANNEL,
    SOCKET_CHANNEL
};

#endif //KRONOS_NETWORK_H
