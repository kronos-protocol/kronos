#include "kronos_server.h"
#include "kronos.h"
#include "kronos_log.h"

#include "server_internal.h"
#include "connection_map_internal.h"
#include "net_send_internal.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>


static volatile LONG s_next_connection_id = 0;

static uint32_t s_generate_connection_id(void) {
    return (uint32_t)InterlockedIncrement(&s_next_connection_id);
}

static void s_send_frame(UDPSocketRef_t socket, const PortAddress_t* addr,
                         Channel_t channel, FrameType_e type,
                         const uint8_t* body, uint16_t body_len) {
    FrameBuilder_c* builder = krs_frame_builder_create(channel, type);
    if (!builder) return;

    if (body && body_len > 0) {
        krs_frame_builder_set_data(builder, body, body_len);
    }

    uint8_t buf[KRONOS_BUFFER_SIZE];
    uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
    krs_frame_builder_destroy(&builder);

    if (n == 0) return;

    krs_net_send_frame(socket, addr, buf, n);
}

void krs_server_handle_connection_frame(ServerPortManager_t* spm,
                                        UDPSocketDescriptor_t* descriptor,
                                        const Frame_t* frame,
                                        const PortAddress_t* remote_addr) {
    if (!spm || !descriptor || !frame || !remote_addr) return;

    Channel_t requested_channel = (frame->body && frame->body_length >= 1) ? frame->body[0] : 0;

    ChannelState_t* state = &descriptor->channel_states[requested_channel];
    AcquireSRWLockExclusive(&descriptor->state_lock);
    if (!state->connections) {
        state->connections = krs_array_create(8);
    }
    if (state->connections) {
        uint32_t count = krs_array_length(state->connections);
        for (uint32_t i = 0; i < count; i++) {
            ClientConnection_t* existing = KRS_ARRAY_GET(state->connections, i, ClientConnection_t);
            if (existing && krs_network_port_address_equals(&existing->remote_address, remote_addr)) {
                existing->last_heartbeat_ms = GetTickCount64();
                uint32_t existing_id = existing->connection_id;
                ReleaseSRWLockExclusive(&descriptor->state_lock);

                uint8_t ack_body[4];
                ack_body[0] = (uint8_t)((existing_id >> 24) & 0xFF);
                ack_body[1] = (uint8_t)((existing_id >> 16) & 0xFF);
                ack_body[2] = (uint8_t)((existing_id >> 8) & 0xFF);
                ack_body[3] = (uint8_t)(existing_id & 0xFF);
                s_send_frame(descriptor->udp_socket_ref, remote_addr, 0, SOCKET_ACK, ack_body, sizeof(ack_body));
                return;
            }
        }
    }

    ClientConnection_t* conn = malloc(sizeof(ClientConnection_t));
    if (!conn) {
        ReleaseSRWLockExclusive(&descriptor->state_lock);
        KRS_LOG_ERROR("server_conn_handler", "ClientConnection allocation failed on channel %u",
                      requested_channel);
        return;
    }

    conn->connection_id = s_generate_connection_id();
    conn->remote_address = *remote_addr;
    conn->last_heartbeat_ms = GetTickCount64();
    conn->congestion = NULL;
    conn->ack_tracker = NULL;
    InitializeSRWLock(&conn->ack_lock);
    InitializeSRWLock(&conn->cc_lock);
    conn->refcount = 1;

    if (!state->connections || krs_array_push(state->connections, conn).base.error_code != KRS_SUCCESS) {
        ReleaseSRWLockExclusive(&descriptor->state_lock);
        KRS_LOG_ERROR("server_conn_handler", "failed to register connection on channel %u (push or NULL list)",
                      requested_channel);
        free(conn);
        return;
    }
    ReleaseSRWLockExclusive(&descriptor->state_lock);

    if (spm->connection_map) {
        AcquireSRWLockExclusive(&spm->connection_map->lock);
        krs_connection_map_put(spm->connection_map, conn->connection_id, descriptor, conn);
        ReleaseSRWLockExclusive(&spm->connection_map->lock);
    }

    uint8_t ack_body[4];
    ack_body[0] = (uint8_t)((conn->connection_id >> 24) & 0xFF);
    ack_body[1] = (uint8_t)((conn->connection_id >> 16) & 0xFF);
    ack_body[2] = (uint8_t)((conn->connection_id >> 8) & 0xFF);
    ack_body[3] = (uint8_t)(conn->connection_id & 0xFF);

    s_send_frame(descriptor->udp_socket_ref, remote_addr, 0, SOCKET_ACK, ack_body, sizeof(ack_body));

    if (descriptor->connect_callback) {
        descriptor->connect_callback(conn->connection_id, requested_channel,
                                     descriptor->connect_callback_user_data);
    }
}

void krs_server_handle_heartbeat_frame(UDPSocketDescriptor_t* descriptor,
                                       const Frame_t* frame,
                                       const PortAddress_t* remote_addr) {
    if (!descriptor || !frame || !remote_addr) return;

    uint64_t now = GetTickCount64();

    AcquireSRWLockShared(&descriptor->state_lock);
    for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
        KrsArray_t* conns = descriptor->channel_states[ch].connections;
        if (!conns) continue;
        uint32_t count = krs_array_length(conns);
        for (uint32_t i = 0; i < count; i++) {
            ClientConnection_t* conn = KRS_ARRAY_GET(conns, i, ClientConnection_t);
            if (!conn) continue;
            if (krs_network_port_address_equals(&conn->remote_address, remote_addr)) {
                conn->last_heartbeat_ms = now;
            }
        }
    }
    ReleaseSRWLockShared(&descriptor->state_lock);
}
