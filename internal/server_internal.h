#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H
#include "network_internal.h"
#include <kronos_port_table.h>
#include <time.h>

typedef struct ServerPort ServerPort_t;

struct ServerPortManager {
    PortTable_t* port_table;
    Address_t default_address;
    Channel_t default_max_channel;
};

struct ServerPort {
    Address_t address;
    Channel_t max_channel;
};

struct ClientConnection {
    struct tm timestamp_opened;
};

struct SocketHandler {
    UDPSocketRef_t udp_socket_ref;
    ChannelHandler_t channel_handlers[]; // Can be changed in future to have variable size
};

struct ChannelHandler {
    ClientConnection_t client_connection;
    enum ChannelType channel_type;
};

SocketHandler_t* krs_server_socket_handler_create();

ChannelHandler_t* krs_server_channel_handler_create();
ChannelHandler_t krs_server_channel_handler_create_st();

bool krs_server_port_manager_validate(ServerPortManager_t* server_port_manager);

#endif //SERVER_INTERNAL_H
