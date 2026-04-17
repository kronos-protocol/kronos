#include "kronos_server.h"
#include "kronos.h"
#include "kronos_fragment.h"
#include "kronos_packet_counter.h"
#include "kronos_ack.h"
#include "kronos_congestion.h"

#include "server_internal.h"
#include "frame_metadata.h"
#include "connection_map_internal.h"

#include <stdlib.h>

#include <winsock2.h>


static void s_send_raw_frame(UDPSocketRef_t socket, const PortAddress_t* addr,
                             const uint8_t* frame_data, uint16_t frame_size) {
    WSABUF wsabuf;
    wsabuf.len = frame_size;
    wsabuf.buf = (char*)frame_data;
    DWORD sent;
    WSASendTo(socket, &wsabuf, 1, &sent, 0,
              (const struct sockaddr*)addr, sizeof(*addr), NULL, NULL);
}

static void s_send_to_connection(UDPSocketRef_t socket, const ClientConnection_t* conn,
                                 Channel_t channel, uint64_t packet_id,
                                 const uint8_t* data, uint16_t length) {
    if (length <= KRS_MAX_PAYLOAD_PER_FRAGMENT) {
        FrameBuilder_c* builder = krs_frame_builder_create(channel, BASIC_MESSAGE);
        if (!builder) return;

        krs_frame_builder_set_packet_id(builder, packet_id);
        krs_frame_builder_set_data(builder, data, length);

        uint8_t buf[KRONOS_BUFFER_SIZE];
        uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
        krs_frame_builder_destroy(&builder);

        if (n == 0) return;

        s_send_raw_frame(socket, &conn->remote_address, buf, n);
    } else {
        FragmentResult_t frag = krs_fragment_split(channel, BASIC_MESSAGE, packet_id,
                                                    data, length, KRS_DEFAULT_MTU, 0);
        if (frag.base.error_code != KRS_SUCCESS) return;

        for (uint16_t f = 0; f < frag.fragment_count; f++) {
            s_send_raw_frame(socket, &conn->remote_address, frag.fragments[f], frag.fragment_sizes[f]);
        }
        krs_fragment_result_destroy(&frag);
    }
}

static void s_broadcast_on_descriptor(UDPSocketDescriptor_t* desc, Channel_t channel,
                                      const uint8_t* data, uint16_t length,
                                      uint32_t exclude_id) {
    ChannelState_t* bc_state = &desc->channel_states[channel];
    AcquireSRWLockExclusive(&bc_state->channel_lock);

    PacketCounter_t* counter = bc_state->packet_counter;
    if (!counter) {
        bc_state->packet_counter = krs_packet_counter_create();
        counter = bc_state->packet_counter;
    }

    KrsArray_t* conns = bc_state->connections;
    if (!conns) {
        ReleaseSRWLockExclusive(&bc_state->channel_lock);
        return;
    }

    uint32_t count = krs_array_length(conns);
    for (uint32_t i = 0; i < count; i++) {
        ClientConnection_t* conn = KRS_ARRAY_GET(conns, i, ClientConnection_t);
        if (!conn) continue;
        if (conn->connection_id == exclude_id) continue;
        uint64_t pid = counter ? krs_packet_counter_next(counter, channel) : 0;
        s_send_to_connection(desc->udp_socket_ref, conn, channel, pid, data, length);
    }

    ReleaseSRWLockExclusive(&bc_state->channel_lock);
}

void krs_server_broadcast(ServerPortManager_t* spm, Channel_t channel,
                          const uint8_t* data, uint16_t length) {
    if (!spm || !data || !spm->descriptor_list) return;

    uint32_t count = krs_array_length(spm->descriptor_list);
    for (uint32_t i = 0; i < count; i++) {
        UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, i, UDPSocketDescriptor_t);
        if (!desc) continue;
        s_broadcast_on_descriptor(desc, channel, data, length, 0);
    }
}

void krs_server_broadcast_except(ServerPortManager_t* spm, Channel_t channel,
                                 uint32_t exclude_connection_id,
                                 const uint8_t* data, uint16_t length) {
    if (!spm || !data || !spm->descriptor_list) return;

    uint32_t count = krs_array_length(spm->descriptor_list);
    for (uint32_t i = 0; i < count; i++) {
        UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, i, UDPSocketDescriptor_t);
        if (!desc) continue;
        s_broadcast_on_descriptor(desc, channel, data, length, exclude_connection_id);
    }
}

Void_r krs_server_send(ServerPortManager_t* spm, uint32_t connection_id, Channel_t channel,
                       const uint8_t* data, uint16_t length, bool require_ack) {
    Void_r result = {0};

    if (!spm || !data) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm or data is NULL");
        return result;
    }

    if (!spm->connection_map) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no connection map");
        return result;
    }

    AcquireSRWLockShared(&spm->connection_map->lock);
    ConnectionMapEntry_t* entry = krs_connection_map_get(spm->connection_map, connection_id);
    if (!entry) {
        ReleaseSRWLockShared(&spm->connection_map->lock);
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "connection_id not found");
        return result;
    }

    UDPSocketDescriptor_t* desc = entry->descriptor;
    PortAddress_t addr = entry->connection->remote_address;
    ReleaseSRWLockShared(&spm->connection_map->lock);

    ChannelState_t* send_ch = &desc->channel_states[channel];
    AcquireSRWLockExclusive(&send_ch->channel_lock);

    PacketCounter_t* counter = send_ch->packet_counter;
    if (!counter) {
        send_ch->packet_counter = krs_packet_counter_create();
        counter = send_ch->packet_counter;
    }

    uint64_t packet_id = counter ? krs_packet_counter_next(counter, channel) : 0;

    AckTracker_t* tracker = NULL;
    if (require_ack) {
        tracker = send_ch->ack_tracker;
        if (!tracker) {
            send_ch->ack_tracker = krs_ack_tracker_create(1000, 5);
            tracker = send_ch->ack_tracker;
        }

        AcquireSRWLockShared(&spm->connection_map->lock);
        entry = krs_connection_map_get(spm->connection_map, connection_id);
        ClientConnection_t* target = entry ? entry->connection : NULL;
        ReleaseSRWLockShared(&spm->connection_map->lock);

        if (target) {
            if (!target->congestion) {
                target->congestion = krs_congestion_create();
            }
            if (target->congestion && !krs_congestion_can_send(target->congestion)) {
                ReleaseSRWLockExclusive(&send_ch->channel_lock);
                result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_CONGESTION_WINDOW_FULL,
                                                               "congestion window full");
                return result;
            }
            if (target->congestion) {
                krs_congestion_on_send(target->congestion);
            }
        }
    }

    ReleaseSRWLockExclusive(&send_ch->channel_lock);

    if (length <= KRS_MAX_PAYLOAD_PER_FRAGMENT) {
        FrameBuilder_c* builder = krs_frame_builder_create(channel, BASIC_MESSAGE);
        if (!builder) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "builder alloc failed");
            return result;
        }
        krs_frame_builder_set_packet_id(builder, packet_id);
        krs_frame_builder_set_data(builder, data, length);
        if (require_ack) {
            krs_frame_builder_set_flag(builder, META_FLAG_ACK_REQUIRED);
        }

        uint8_t buf[KRONOS_BUFFER_SIZE];
        uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
        krs_frame_builder_destroy(&builder);

        if (n == 0) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NETWORK_SOCKET_ERROR, "serialize failed");
            return result;
        }

        if (require_ack && tracker) {
            AcquireSRWLockExclusive(&send_ch->channel_lock);
            krs_ack_tracker_expect(tracker, packet_id, buf, n);
            ReleaseSRWLockExclusive(&send_ch->channel_lock);
        }

        s_send_raw_frame(desc->udp_socket_ref, &addr, buf, n);
        InterlockedIncrement64(&spm->stat_messages_sent);
    } else {
        FragmentResult_t frag = krs_fragment_split(channel, BASIC_MESSAGE, packet_id,
                                                   data, length, KRS_DEFAULT_MTU,
                                                   (uint16_t)(require_ack ? (1u << META_FLAG_ACK_REQUIRED) : 0));
        if (frag.base.error_code != KRS_SUCCESS) {
            result.base = frag.base;
            return result;
        }

        if (require_ack && tracker) {
            AcquireSRWLockExclusive(&send_ch->channel_lock);
            for (uint16_t f = 0; f < frag.fragment_count; f++) {
                krs_ack_tracker_expect(tracker, packet_id, frag.fragments[f], frag.fragment_sizes[f]);
            }
            ReleaseSRWLockExclusive(&send_ch->channel_lock);
        }

        for (uint16_t f = 0; f < frag.fragment_count; f++) {
            s_send_raw_frame(desc->udp_socket_ref, &addr, frag.fragments[f], frag.fragment_sizes[f]);
            InterlockedIncrement64(&spm->stat_messages_sent);
        }
        krs_fragment_result_destroy(&frag);
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_send_blocking(ServerPortManager_t* spm, uint32_t connection_id,
                                Channel_t channel, const uint8_t* data,
                                uint16_t length, uint32_t timeout_ms) {
    uint64_t deadline = GetTickCount64() + timeout_ms;

    for (;;) {
        Void_r result = krs_server_send(spm, connection_id, channel, data, length, true);
        if (result.base.valid) return result;
        if (result.base.error_code != KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) return result;
        if (GetTickCount64() >= deadline) return result;
        Sleep(1);
    }
}
