#ifndef KRONOS_SERVER_H
#define KRONOS_SERVER_H
#include "kronos_network.h"

typedef struct EndpointManager EndpointManager_t;
typedef struct ClientConnection ClientConnection_t;

Endpoint_t* krs_endpoint_setup(Address_t address, Channel_t max_channel);

#endif //KRONOS_SERVER_H
