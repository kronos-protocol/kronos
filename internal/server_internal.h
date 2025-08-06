#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H
#include "network_internal.h"
#include <kronos_port_table.h>
#include <time.h>

typedef struct ServerPort ServerPort_t;

struct ServerPortManager {
    PortTable_t* open_ports;
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

struct SocketManager {
    UDPSocketRef_t udp_socket_ref;
    ChannelHandler_t* channel_handlers;
};

struct ChannelHandler {
    ClientConnection_t* client_connection;
    enum ChannelType channel_type;
};

boolean krs_server_port_manager_validate(ServerPortManager_t* server_port_manager);

#endif //SERVER_INTERNAL_H
