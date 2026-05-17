#include "kronos_server.h"
#include "kronos_log.h"

#include "server_internal.h"
#include "connection_map_internal.h"

#include <stdlib.h>


UDPSocketDescriptor_t* krs_server_udp_socket_handler_create(PortAddress_t port_address) {
    UDPSocketDescriptor_t* udp_socket_handler = calloc(1, sizeof(UDPSocketDescriptor_t));
    if (!udp_socket_handler) {
        KRS_LOG_ERROR("socket_handler", "descriptor allocation failed");
        return NULL;
    }

    InitializeSRWLock(&udp_socket_handler->state_lock);
    for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
        InitializeSRWLock(&udp_socket_handler->channel_states[ch].channel_lock);
    }
    udp_socket_handler->udp_socket_ref = krs_network_udp_socket_ref_create(port_address);
    if (udp_socket_handler->udp_socket_ref == INVALID_SOCKET) {
        KRS_LOG_ERROR("socket_handler", "UDP socket creation failed for port %u",
                      port_address.sin6_port);
        free(udp_socket_handler);
        return NULL;
    }
    return udp_socket_handler;
}

void krs_server_udp_socket_handler_destroy(UDPSocketDescriptor_t** socket_handler) {
    if (!socket_handler || !*socket_handler) return;

    UDPSocketDescriptor_t* desc = *socket_handler;

    KrsArray_t* anchor = desc->channel_states[0].connections;
    if (anchor) {
        uint32_t count = krs_array_length(anchor);
        for (uint32_t i = 0; i < count; i++) {
            ClientConnection_t* conn = KRS_ARRAY_GET(anchor, i, ClientConnection_t);
            krs_connection_map_release(conn);
        }
    }
    for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
        ChannelState_t* state = &desc->channel_states[ch];
        krs_packet_counter_destroy(&state->packet_counter);
        krs_reassembler_destroy(&state->reassembler);
        if (state->connections) {
            krs_array_destroy(&state->connections);
        }
    }

    if (desc->udp_socket_ref != INVALID_SOCKET) {
        closesocket(desc->udp_socket_ref);
    }
    free(desc);
    *socket_handler = NULL;
}
