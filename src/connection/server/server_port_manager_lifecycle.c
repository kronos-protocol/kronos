#include "kronos_network.h"
#include "../internal/network_internal.h"
#include "kronos_server.h"
#include "../internal/server_internal.h"


// ALERT: Legacy code used for reference
Endpoint_t* krs_endpoint_setup(Address_t address, Channel_t max_channel) {
    Endpoint_t * endpoint = malloc(sizeof(Endpoint_t));
    endpoint->address = address;
    endpoint->max_channel = max_channel;

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    UDPSocketRef_t udp_socket_ref = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket_ref == INVALID_SOCKET) {
        //TODO: error handling
    }
    endpoint->udp_socket_ref = udp_socket_ref;
    bind(udp_socket_ref, (struct sockaddr*)&endpoint->address, sizeof(endpoint->address));
    return endpoint;
}

ServerPortManager_t* krs_server_port_manager_create(const Address_t default_address) {
    ServerPortManager_t* server_port_manager = malloc(sizeof(ServerPortManager_t));
    ServerPort_t* open_ports = malloc(255 * sizeof(ServerPort_t));

    server_port_manager->open_ports = open_ports;
    server_port_manager->open_ports_length = 0;
    server_port_manager->default_address = default_address;
    server_port_manager->default_max_channel = 255; // TODO: let this be changed for now max Channel is always uint8_t max value

    return server_port_manager;
}

void krs_server_port_manager_port_add(ServerPortManager_t* server_port_manager, Port_t port) {
    if (!krs_server_port_manager_validate(server_port_manager)) {
        //TODO: handle errors
    }

    PortAddress_t port_address;

}


void krs_server_port_manager_port_add_with_address(ServerPortManager_t* server_port_manager, Port_t port, Address_t address);


