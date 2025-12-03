#ifndef KRONOS_SERVER_H
#define KRONOS_SERVER_H
#include "kronos_network.h"

typedef struct ServerPortManager ServerPortManager_t;
typedef struct UdpSocketHandler UdpSocketHandler_t;
typedef struct ChannelHandler ChannelHandler_t;
typedef struct ClientConnection ClientConnection_t;

ServerPortManager_t* krs_server_port_manager_create(Address_t default_address);
void krs_server_port_manager_port_add(ServerPortManager_t* server_port_manager, Port_t port);
void krs_server_port_manager_port_add_with_address(ServerPortManager_t* server_port_manager, Port_t port, Address_t address);

UdpSocketHandler_t* krs_server_udp_socket_handler_create(PortAddress_t port_address);
void krs_server_udp_socket_handler_destroy(UdpSocketHandler_t** socket_handler);

#endif //KRONOS_SERVER_H