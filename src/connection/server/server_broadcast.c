#include "kronos_server.h"
#include "kronos.h"
#include "kronos_fragment.h"
#include "kronos_packet_counter.h"
#include "kronos_ack.h"
#include "kronos_congestion.h"

#include "server_internal.h"
#include "frame_metadata.h"
#include "connection_map_internal.h"
#include "net_send_internal.h"

#include <stdlib.h>

#include <winsock2.h>


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

        krs_net_send_frame(socket, &conn->remote_address, buf, n);
    } else {
        FragmentResult_t frag = krs_fragment_split(channel, BASIC_MESSAGE, packet_id,
                                                    data, length, KRS_DEFAULT_MTU, 0);
        if (frag.base.error_code != KRS_SUCCESS) return;

        for (uint16_t f = 0; f < frag.fragment_count; f++) {
            krs_net_send_frame(socket, &conn->remote_address, frag.fragments[f], frag.fragment_sizes[f]);
        }
        krs_fragment_result_destroy(&frag);
    }
}

static void s_broadcast_on_descriptor(UDPSocketDescriptor_t* desc, Channel_t channel,
                                      const uint8_t* data, uint16_t length,
                                      uint32_t exclude_id) {
    ClientConnection_t* snapshot[256];
    uint64_t pids[256];
    uint32_t snap_count = 0;

    AcquireSRWLockShared(&desc->state_lock);
    KrsArray_t* conns = desc->channel_states[channel].connections;
    if (!conns) {
        ReleaseSRWLockShared(&desc->state_lock);
        return;
    }
    uint32_t count = krs_array_length(conns);
    for (uint32_t i = 0; i < count && snap_count < 256; i++) {
        ClientConnection_t* conn = KRS_ARRAY_GET(conns, i, ClientConnection_t);
        if (!conn) continue;
        if (conn->connection_id == exclude_id) continue;
        InterlockedIncrement(&conn->refcount);
        snapshot[snap_count++] = conn;
    }
    ReleaseSRWLockShared(&desc->state_lock);

    ChannelState_t* bc_state = &desc->channel_states[channel];
    AcquireSRWLockExclusive(&bc_state->channel_lock);
    if (!bc_state->packet_counter) {
        bc_state->packet_counter = krs_packet_counter_create();
    }
    for (uint32_t i = 0; i < snap_count; i++) {
        pids[i] = bc_state->packet_counter
            ? krs_packet_counter_next(bc_state->packet_counter, channel)
            : 0;
    }
    ReleaseSRWLockExclusive(&bc_state->channel_lock);

    for (uint32_t i = 0; i < snap_count; i++) {
        s_send_to_connection(desc->udp_socket_ref, snapshot[i], channel, pids[i], data, length);
    }

    for (uint32_t i = 0; i < snap_count; i++) {
        krs_connection_map_release(snapshot[i]);
    }
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

Void_r krs_server_broadcast_reliable(ServerPortManager_t* spm, Channel_t channel,
                                     const uint8_t* data, uint16_t length) {
    Void_r result = {0};
    if (!spm || !data) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm or data NULL");
        return result;
    }
    if (!spm->descriptor_list) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no descriptor list");
        return result;
    }

    uint32_t desc_count = krs_array_length(spm->descriptor_list);
    for (uint32_t di = 0; di < desc_count; di++) {
        UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, di, UDPSocketDescriptor_t);
        if (!desc) continue;

        uint32_t snap_ids[256];
        uint32_t snap_count = 0;

        AcquireSRWLockShared(&desc->state_lock);
        KrsArray_t* subs = desc->channel_states[channel].connections;
        if (subs) {
            uint32_t count = krs_array_length(subs);
            for (uint32_t i = 0; i < count && snap_count < 256; i++) {
                ClientConnection_t* c = KRS_ARRAY_GET(subs, i, ClientConnection_t);
                if (c) {
                    snap_ids[snap_count++] = c->connection_id;
                }
            }
        }
        ReleaseSRWLockShared(&desc->state_lock);

        for (uint32_t i = 0; i < snap_count; i++) {
            (void)krs_server_send(spm, snap_ids[i], channel, data, length, true);
        }
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
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

    UDPSocketDescriptor_t* desc = NULL;
    ClientConnection_t* target = NULL;
    if (!krs_connection_map_acquire(spm->connection_map, connection_id, &desc, &target)) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "connection_id not found");
        return result;
    }

    PortAddress_t addr = target->remote_address;

    ChannelState_t* send_ch = &desc->channel_states[channel];
    AcquireSRWLockExclusive(&send_ch->channel_lock);
    PacketCounter_t* counter = send_ch->packet_counter;
    if (!counter) {
        send_ch->packet_counter = krs_packet_counter_create();
        counter = send_ch->packet_counter;
    }
    uint64_t packet_id = counter ? krs_packet_counter_next(counter, channel) : 0;
    ReleaseSRWLockExclusive(&send_ch->channel_lock);

    AckTracker_t* tracker = NULL;
    if (require_ack) {
        AcquireSRWLockExclusive(&target->ack_lock);
        if (!target->ack_tracker) {
            target->ack_tracker = krs_ack_tracker_create(1000, 5);
        }
        tracker = target->ack_tracker;
        ReleaseSRWLockExclusive(&target->ack_lock);

        if (!target->congestion) {
            CongestionController_t* new_cc = krs_congestion_create();
            if (new_cc) {
                CongestionController_t* prev = (CongestionController_t*)InterlockedCompareExchangePointer(
                    (PVOID*)&target->congestion, new_cc, NULL);
                if (prev != NULL) {
                    krs_congestion_destroy(&new_cc);
                }
            }
        }

        bool can_send = true;
        if (target->congestion) {
            AcquireSRWLockExclusive(&target->cc_lock);
            can_send = krs_congestion_can_send(target->congestion);
            if (can_send) {
                krs_congestion_on_send(target->congestion);
            }
            ReleaseSRWLockExclusive(&target->cc_lock);
        }
        if (!can_send) {
            krs_connection_map_release(target);
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_CONGESTION_WINDOW_FULL,
                                                           "congestion window full");
            return result;
        }
    }

    if (length <= KRS_MAX_PAYLOAD_PER_FRAGMENT) {
        FrameBuilder_c* builder = krs_frame_builder_create(channel, BASIC_MESSAGE);
        if (!builder) {
            krs_connection_map_release(target);
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
            krs_connection_map_release(target);
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NETWORK_SOCKET_ERROR, "serialize failed");
            return result;
        }

        if (require_ack && tracker) {
            AcquireSRWLockExclusive(&target->ack_lock);
            krs_ack_tracker_expect(tracker, packet_id, channel, buf, n);
            ReleaseSRWLockExclusive(&target->ack_lock);
        }

        krs_net_send_frame(desc->udp_socket_ref, &addr, buf, n);
        InterlockedIncrement64(&spm->stat_messages_sent);
    } else {
        FragmentResult_t frag = krs_fragment_split(channel, BASIC_MESSAGE, packet_id,
                                                   data, length, KRS_DEFAULT_MTU,
                                                   (uint16_t)(require_ack ? (1u << META_FLAG_ACK_REQUIRED) : 0));
        if (frag.base.error_code != KRS_SUCCESS) {
            krs_connection_map_release(target);
            result.base = frag.base;
            return result;
        }

        if (require_ack && tracker) {
            AcquireSRWLockExclusive(&target->ack_lock);
            for (uint16_t f = 0; f < frag.fragment_count; f++) {
                krs_ack_tracker_expect(tracker, packet_id, channel,
                                       frag.fragments[f], frag.fragment_sizes[f]);
            }
            ReleaseSRWLockExclusive(&target->ack_lock);
        }

        for (uint16_t f = 0; f < frag.fragment_count; f++) {
            krs_net_send_frame(desc->udp_socket_ref, &addr, frag.fragments[f], frag.fragment_sizes[f]);
            InterlockedIncrement64(&spm->stat_messages_sent);
        }
        krs_fragment_result_destroy(&frag);
    }

    krs_connection_map_release(target);

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_send_blocking(ServerPortManager_t* spm, uint32_t connection_id,
                                Channel_t channel, const uint8_t* data,
                                uint16_t length, uint32_t timeout_ms) {
    uint64_t deadline = GetTickCount64() + timeout_ms;
    uint32_t backoff_ms = 1;

    for (;;) {
        Void_r result = krs_server_send(spm, connection_id, channel, data, length, true);
        if (result.base.valid) return result;
        if (result.base.error_code != KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) return result;

        uint64_t now = GetTickCount64();
        if (now >= deadline) return result;

        uint64_t remaining = deadline - now;
        DWORD sleep_ms = (DWORD)(backoff_ms < remaining ? backoff_ms : remaining);
        Sleep(sleep_ms);

        if (backoff_ms < 50) backoff_ms *= 2;
    }
}
