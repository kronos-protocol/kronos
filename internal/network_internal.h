#ifndef NETWORK_INTERNAL_H
#define NETWORK_INTERNAL_H
#include <stdint.h>
#include "kronos_network.h"
#include "kronos_server.h"
#include "kronos_client.h"

// Network stack for Windows
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <psdk_inc/_wsadata.h>
#include <ws2ipdef.h>

struct Endpoint {
    Address_t address;
    UDPSocketRef_t udp_socket_ref;
    Channel_t channel;
};

struct Connection {
    ClientConnection_t* client_connection;
    ServerConnection_t* server_connection;
};

#endif //NETWORK_INTERNAL_H
