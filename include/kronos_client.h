#ifndef KRONOS_CLIENT_H
#define KRONOS_CLIENT_H

#include "kronos_network.h"

typedef struct ServerConnection ServerConnection_t;

ServerConnection_t* krs_client_server_connect(Endpoint_t endpoint);

#endif //KRONOS_CLIENT_H
