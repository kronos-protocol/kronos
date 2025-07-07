#ifndef NETWORK_INTERNAL_H
#define NETWORK_INTERNAL_H
#include <stdint.h>
#include "kronos_network.h"
#include "kronos_server.h"
#include "kronos_client.h"

#include <ws2ipdef.h>

struct Endpoint {
    Address_t address;
    UDPSocketRef_t udp_socket_ref;
    Channel_t max_channel;
};

struct Connection {
    ClientConnection_t* client_connection;
    ServerConnection_t* server_connection;
};

#endif //NETWORK_INTERNAL_H
