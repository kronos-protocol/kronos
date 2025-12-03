#import <kronos_server.h>
#import <server_internal.h>


UdpSocketHandler_t* krs_server_udp_socket_handler_create(PortAddress_t port_address) {
    UdpSocketHandler_t* udp_socket_handler = calloc(1, sizeof(UdpSocketHandler_t));
    if (!udp_socket_handler) return NULL;

    udp_socket_handler->udp_socket_ref = krs_network_udp_socket_ref_create(port_address);

    return udp_socket_handler;
}