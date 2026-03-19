#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H

#include "kronos_client.h"
#include "kronos_network.h"

#include <stdbool.h>
#include <stdint.h>

struct ServerConnection {
    UDPSocketRef_t socket;
    PortAddress_t  server_address;
    uint32_t       connection_id;
    bool           connected;
};

#endif // CLIENT_INTERNAL_H
