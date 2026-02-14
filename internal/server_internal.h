#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H
#include "network_internal.h"
#include <kronos_port_table.h>
#include <time.h>


struct ServerPortManager {
    PortTable_t* port_table;
    Address_t default_address;
    Channel_t default_max_channel;
};

struct ClientConnection {
    struct tm timestamp_opened;
};

struct UDPSocketDescriptor {
    UDPSocketRef_t udp_socket_ref;
    ChannelType_e channel_types[MAX_CHANNEL_NUMBER];
};

// Can be used in future in UdpSocketHandler instead of a raw ChannelType_e array
struct ChannelDescriptor {
    ChannelType_e channel_type;
};

ChannelHandler_t* krs_server_channel_handler_create();

bool krs_server_port_manager_validate(ServerPortManager_t* server_port_manager);

#endif //SERVER_INTERNAL_H
