#include "kronos_network.h"
#include "../internal/network_internal.h"

#include <stdlib.h>
#include <winsock2.h>
#include <psdk_inc/_wsadata.h>

Endpoint_t* krs_endpoint_setup(Address_t address, Channel_t max_channel) {
    Endpoint_t* endpoint;
    endpoint = malloc(sizeof(Endpoint_t));
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

