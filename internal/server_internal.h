#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H
#include "network_internal.h"

typedef struct ServerPort ServerPort_t;

struct ServerPortManager {
    ServerPort_t* open_ports;
    Port_t open_ports_length;
    Address_t default_address;
    Channel_t default_max_channel;
};

struct ServerPort {
    Address_t address;
    Channel_t max_channel;
};

struct ClientConnection {

};

boolean krs_server_port_manager_validate(ServerPortManager_t* server_port_manager);

#endif //SERVER_INTERNAL_H
