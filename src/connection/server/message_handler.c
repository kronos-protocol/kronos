#include "kronos_server.h"
#include "kronos.h"
#include "kronos_ack.h"
#include "kronos_congestion.h"
#include "kronos_log.h"

#include "server_internal.h"
#include "iocp_internal.h"
#include "frame_metadata.h"
#include "message_pool_internal.h"
#include "connection_map_internal.h"
#include "net_send_internal.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>


void krs_server_finalize_orphan_evictions(ServerPortManager_t* spm,
                                          UDPSocketDescriptor_t* desc,
                                          ClientConnection_t** unique_orphans,
                                          uint32_t orphan_count) {
    if (orphan_count == 0) return;

    uint32_t evicted_ids[MAX_EVICTIONS_PER_CYCLE];
    for (uint32_t u = 0; u < orphan_count; u++) {
        evicted_ids[u] = unique_orphans[u]->connection_id;
    }

    if (spm->connection_map) {
        AcquireSRWLockExclusive(&spm->connection_map->lock);
        for (uint32_t u = 0; u < orphan_count; u++) {
            krs_connection_map_remove(spm->connection_map, evicted_ids[u]);
        }
        ReleaseSRWLockExclusive(&spm->connection_map->lock);
    }

    for (uint32_t u = 0; u < orphan_count; u++) {
        krs_connection_map_release(unique_orphans[u]);
    }

    InterlockedAdd64(&spm->stat_disconnections, orphan_count);

    if (desc->disconnect_callback) {
        for (uint32_t u = 0; u < orphan_count; u++) {
            desc->disconnect_callback(evicted_ids[u], 0,
                                      desc->disconnect_callback_user_data);
        }
    }
}

static void s_send_ack_response(UDPSocketDescriptor_t* desc, const PortAddress_t* remote_addr,
                                Channel_t channel, uint64_t packet_id) {
    FrameBuilder_c* builder = krs_frame_builder_create(channel, MESSAGE_ACK);
    if (!builder) return;
    krs_frame_builder_set_packet_id(builder, packet_id);

    uint8_t buf[64];
    uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
    krs_frame_builder_destroy(&builder);
    if (n == 0) return;

    krs_net_send_frame(desc->udp_socket_ref, remote_addr, buf, n);
}

static void s_dispatch_connection(ServerPortManager_t* spm, UDPSocketDescriptor_t* desc,
                                  const Frame_t* frame, const PortAddress_t* remote_addr) {
    krs_server_handle_connection_frame(spm, desc, frame, remote_addr);
    InterlockedIncrement64(&spm->stat_connections_total);
}

static void s_dispatch_heartbeat(UDPSocketDescriptor_t* desc,
                                 const Frame_t* frame, const PortAddress_t* remote_addr) {
    krs_server_handle_heartbeat_frame(desc, frame, remote_addr);
}

static void s_dispatch_disconnect(ServerPortManager_t* spm, UDPSocketDescriptor_t* desc,
                                  const PortAddress_t* remote_addr) {
    ClientConnection_t* unique_orphans[MAX_EVICTIONS_PER_CYCLE];
    uint32_t orphan_count = 0;

    AcquireSRWLockExclusive(&desc->state_lock);
    for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
        KrsArray_t* conns = desc->channel_states[ch].connections;
        if (!conns) continue;
        uint32_t ci = krs_array_length(conns);
        while (ci > 0) {
            ci--;
            ClientConnection_t* c = KRS_ARRAY_GET(conns, ci, ClientConnection_t);
            if (!c) continue;
            if (!krs_network_port_address_equals(&c->remote_address, remote_addr)) continue;

            bool already_tracked = false;
            for (uint32_t u = 0; u < orphan_count; u++) {
                if (unique_orphans[u] == c) { already_tracked = true; break; }
            }
            if (!already_tracked && orphan_count < MAX_EVICTIONS_PER_CYCLE) {
                unique_orphans[orphan_count++] = c;
            }
            krs_array_remove(conns, ci);
        }
    }
    ReleaseSRWLockExclusive(&desc->state_lock);

    krs_server_finalize_orphan_evictions(spm, desc, unique_orphans, orphan_count);
}

static void s_dispatch_ack(ServerPortManager_t* spm, UDPSocketDescriptor_t* desc,
                           const Frame_t* frame, const PortAddress_t* remote_addr) {
    (void)desc;
    InterlockedIncrement64(&spm->stat_ack_received);

    if (!spm->connection_map) return;

    AcquireSRWLockShared(&spm->connection_map->lock);
    uint32_t conn_id = krs_connection_map_get_by_address(spm->connection_map, remote_addr);
    ConnectionMapEntry_t* entry = conn_id ? krs_connection_map_get(spm->connection_map, conn_id) : NULL;
    ClientConnection_t* c = entry ? entry->connection : NULL;

    if (!c) {
        ReleaseSRWLockShared(&spm->connection_map->lock);
        return;
    }

    AcquireSRWLockExclusive(&c->ack_lock);
    double rtt_ms = -1.0;
    if (c->ack_tracker) {
        rtt_ms = krs_ack_tracker_receive_rtt(c->ack_tracker, frame->packet_id, frame->channel);
    }
    ReleaseSRWLockExclusive(&c->ack_lock);

    if (rtt_ms >= 0.0 && c->congestion) {
        AcquireSRWLockExclusive(&c->cc_lock);
        krs_congestion_on_ack(c->congestion, rtt_ms);
        uint32_t new_timeout = (uint32_t)krs_congestion_get_rto(c->congestion);
        ReleaseSRWLockExclusive(&c->cc_lock);

        AcquireSRWLockExclusive(&c->ack_lock);
        if (c->ack_tracker) {
            krs_ack_tracker_set_timeout(c->ack_tracker, new_timeout);
        }
        ReleaseSRWLockExclusive(&c->ack_lock);
    }

    ReleaseSRWLockShared(&spm->connection_map->lock);
}

static ClientConnection_t* s_find_conn_on_channel0_unlocked(UDPSocketDescriptor_t* desc,
                                                            const PortAddress_t* remote_addr) {
    KrsArray_t* ch0 = desc->channel_states[0].connections;
    if (!ch0) return NULL;
    uint32_t count = krs_array_length(ch0);
    for (uint32_t i = 0; i < count; i++) {
        ClientConnection_t* c = KRS_ARRAY_GET(ch0, i, ClientConnection_t);
        if (c && krs_network_port_address_equals(&c->remote_address, remote_addr)) {
            return c;
        }
    }
    return NULL;
}

static bool s_array_contains_conn_unlocked(KrsArray_t* arr, ClientConnection_t* needle) {
    if (!arr) return false;
    uint32_t count = krs_array_length(arr);
    for (uint32_t i = 0; i < count; i++) {
        ClientConnection_t* c = KRS_ARRAY_GET(arr, i, ClientConnection_t);
        if (c == needle) return true;
    }
    return false;
}

static void s_dispatch_subscribe(UDPSocketDescriptor_t* desc,
                                 const Frame_t* frame,
                                 const PortAddress_t* remote_addr) {
    if (!frame->body || frame->body_length < 1) return;
    uint8_t target_ch = frame->body[0];
    if (target_ch < 10 || target_ch > MAX_CHANNEL_NUMBER) {
        KRS_LOG_DEBUG("message_handler", "subscribe rejected: target channel %u out of range",
                      target_ch);
        return;
    }

    bool committed = false;
    AcquireSRWLockExclusive(&desc->state_lock);
    ClientConnection_t* conn = s_find_conn_on_channel0_unlocked(desc, remote_addr);
    if (conn) {
        ChannelState_t* target_state = &desc->channel_states[target_ch];
        if (!target_state->connections) {
            target_state->connections = krs_array_create(8);
        }
        if (target_state->connections &&
            s_array_contains_conn_unlocked(target_state->connections, conn)) {
            committed = true;
        } else if (target_state->connections &&
                   krs_array_push(target_state->connections, conn).base.error_code == KRS_SUCCESS) {
            committed = true;
        }
    }
    ReleaseSRWLockExclusive(&desc->state_lock);

    if (!committed) {
        KRS_LOG_DEBUG("message_handler", "subscribe failed for channel %u", target_ch);
        return;
    }

    bool ack_required = (frame->presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) != 0;
    if (ack_required) {
        s_send_ack_response(desc, remote_addr, 0, frame->packet_id);
    }
}

static void s_dispatch_unsubscribe(UDPSocketDescriptor_t* desc,
                                   const Frame_t* frame,
                                   const PortAddress_t* remote_addr) {
    if (!frame->body || frame->body_length < 1) return;
    uint8_t target_ch = frame->body[0];
    if (target_ch < 10 || target_ch > MAX_CHANNEL_NUMBER) return;

    AcquireSRWLockExclusive(&desc->state_lock);
    ClientConnection_t* conn = s_find_conn_on_channel0_unlocked(desc, remote_addr);
    if (conn) {
        KrsArray_t* target = desc->channel_states[target_ch].connections;
        if (target) {
            uint32_t count = krs_array_length(target);
            for (uint32_t i = 0; i < count; i++) {
                ClientConnection_t* c = KRS_ARRAY_GET(target, i, ClientConnection_t);
                if (c == conn) {
                    krs_array_remove(target, i);
                    break;
                }
            }
        }
    }
    ReleaseSRWLockExclusive(&desc->state_lock);

    bool ack_required = (frame->presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) != 0;
    if (ack_required) {
        s_send_ack_response(desc, remote_addr, 0, frame->packet_id);
    }
}

static void s_dispatch_app_frame(ServerPortManager_t* spm, UDPSocketDescriptor_t* desc,
                                 const Frame_t* frame, const PortAddress_t* remote_addr) {
    bool has_fragment_info = (frame->presence_flags & (uint16_t)(1u << META_FLAG_FRAGMENT_INFO)) != 0;

    const uint8_t* callback_data = NULL;
    uint16_t callback_data_length = 0;
    uint8_t* heap_data = NULL;
    bool ack_required = false;

    if (!has_fragment_info) {
        callback_data = frame->body;
        callback_data_length = frame->body_length;
        ack_required = (frame->presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) != 0;
    } else {
        InterlockedIncrement64(&spm->stat_fragments_received);

        ChannelState_t* ch_state = &desc->channel_states[frame->channel];
        AcquireSRWLockExclusive(&ch_state->channel_lock);
        Reassembler_t* reassembler = ch_state->reassembler;
        if (!reassembler) {
            ch_state->reassembler = krs_reassembler_create();
            reassembler = ch_state->reassembler;
        }
        if (!reassembler) {
            ReleaseSRWLockExclusive(&ch_state->channel_lock);
            KRS_LOG_ERROR("message_handler", "reassembler creation failed on channel %u",
                          frame->channel);
            return;
        }
        ReassembleResult_t reassembly = krs_reassembler_feed(reassembler, frame);
        ReleaseSRWLockExclusive(&ch_state->channel_lock);

        if (!reassembly.complete) return;

        InterlockedIncrement64(&spm->stat_fragments_reassembled);

        callback_data = reassembly.data;
        callback_data_length = (uint16_t)reassembly.data_length;
        heap_data = reassembly.data;
        ack_required = reassembly.ack_required;
    }

    if (ack_required) {
        s_send_ack_response(desc, remote_addr, frame->channel, frame->packet_id);
        InterlockedIncrement64(&spm->stat_ack_sent);
    }

    uint32_t conn_id = 0;
    bool subscribed = false;

    AcquireSRWLockShared(&desc->state_lock);
    KrsArray_t* subs = desc->channel_states[frame->channel].connections;
    if (subs) {
        uint32_t count = krs_array_length(subs);
        for (uint32_t i = 0; i < count; i++) {
            ClientConnection_t* c = KRS_ARRAY_GET(subs, i, ClientConnection_t);
            if (c && krs_network_port_address_equals(&c->remote_address, remote_addr)) {
                conn_id = c->connection_id;
                subscribed = true;
                break;
            }
        }
    }
    ReleaseSRWLockShared(&desc->state_lock);

    if (!subscribed) {
        KRS_LOG_DEBUG("message_handler", "drop app frame ch=%u: address not subscribed",
                      frame->channel);
        free(heap_data);
        return;
    }

    ChannelMessageCallback_f cb = desc->channel_callbacks[frame->channel];
    void* ud = desc->channel_callback_user_data[frame->channel];
    if (!cb) {
        cb = desc->port_callback;
        ud = desc->port_callback_user_data;
    }

    if (cb) {
        cb(frame->channel, conn_id, callback_data, callback_data_length, ud);
    }

    free(heap_data);
}

static void s_route_frame(ServerPortManager_t* spm, UDPSocketDescriptor_t* desc,
                          const Frame_t* frame, const PortAddress_t* remote_addr) {
    if (frame->channel == 0 && frame->frame_type == CONNECTION) {
        s_dispatch_connection(spm, desc, frame, remote_addr);
        return;
    }
    if (frame->channel == 1 && frame->frame_type == HEARTBEAT) {
        s_dispatch_heartbeat(desc, frame, remote_addr);
        return;
    }
    if (frame->channel == 0 && frame->frame_type == DISCONNECT) {
        s_dispatch_disconnect(spm, desc, remote_addr);
        return;
    }
    if (frame->channel == 0 && frame->frame_type == SUBSCRIBE) {
        s_dispatch_subscribe(desc, frame, remote_addr);
        return;
    }
    if (frame->channel == 0 && frame->frame_type == UNSUBSCRIBE) {
        s_dispatch_unsubscribe(desc, frame, remote_addr);
        return;
    }
    if (frame->frame_type == MESSAGE_ACK) {
        s_dispatch_ack(spm, desc, frame, remote_addr);
        return;
    }
    if (frame->frame_type != BASIC_MESSAGE) {
        KRS_LOG_DEBUG("message_handler", "drop frame: unknown type %u on channel %u",
                      frame->frame_type, frame->channel);
        return;
    }
    if (frame->channel >= 2 && frame->channel <= 9) {
        return;
    }
    if (frame->channel >= 10) {
        s_dispatch_app_frame(spm, desc, frame, remote_addr);
    }
}

DWORD WINAPI krs_server_message_handler_thread(LPVOID param) {
    ServerPortManager_t* spm = (ServerPortManager_t*)param;

    while (spm->running) {
        IncomingMessage_t* msg = krs_message_queue_pop(spm->message_queue, 100);
        if (!msg) continue;

        PortTableLookup_r lookup = krs_lib_port_table_lookup(spm->port_table, msg->port);
        if (!lookup.exists) {
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        }

        UDPSocketDescriptor_t* desc = lookup.socket_handler;

        uint8_t body_buf[KRONOS_BUFFER_SIZE];
        Frame_t frame = krs_frame_create(msg->data, (uint16_t)msg->data_length,
                                         body_buf, sizeof(body_buf));

        if (frame.protocol_char != 0x4B) {
            KRS_LOG_DEBUG("message_handler", "dropped frame: invalid protocol byte 0x%02X",
                          frame.protocol_char);
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        }
        if (frame.body_length > 0 && frame.body == NULL) {
            KRS_LOG_DEBUG("message_handler", "dropped frame: body_length=%u but body is NULL",
                          frame.body_length);
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        }

        InterlockedIncrement64(&spm->stat_messages_received);

        s_route_frame(spm, desc, &frame, &msg->remote_address);

        krs_message_pool_release(spm->message_pool, msg);
    }

    return 0;
}
