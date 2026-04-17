#include "kronos_server.h"
#include "kronos.h"
#include "kronos_congestion.h"

#include "server_internal.h"
#include "iocp_internal.h"
#include "frame_metadata.h"
#include "message_pool_internal.h"
#include "connection_map_internal.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>

typedef struct {
    uint32_t  connection_id;
    Channel_t channel;
} EvictedConnection_t;

#define MAX_EVICTIONS_PER_CYCLE 64

static void s_send_ack_response(UDPSocketDescriptor_t* desc, const PortAddress_t* remote_addr,
                                Channel_t channel, uint64_t packet_id) {
    FrameBuilder_c* builder = krs_frame_builder_create(channel, MESSAGE_ACK);
    if (!builder) return;
    krs_frame_builder_set_packet_id(builder, packet_id);

    uint8_t buf[64];
    uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
    krs_frame_builder_destroy(&builder);
    if (n == 0) return;

    WSABUF wsabuf;
    wsabuf.len = n;
    wsabuf.buf = (char*)buf;
    DWORD sent;
    WSASendTo(desc->udp_socket_ref, &wsabuf, 1, &sent, 0,
              (const struct sockaddr*)remote_addr, sizeof(*remote_addr), NULL, NULL);
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
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        }

        if (frame.body_length > 0 && frame.body == NULL) {
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        }

        InterlockedIncrement64(&spm->stat_messages_received);

        if (frame.channel == 0 && frame.frame_type == CONNECTION) {
            krs_server_handle_connection_frame(spm, desc, &frame, &msg->remote_address);
            InterlockedIncrement64(&spm->stat_connections_total);
        } else if (frame.channel == 1 && frame.frame_type == HEARTBEAT) {
            krs_server_handle_heartbeat_frame(desc, &frame, &msg->remote_address);
        } else if (frame.channel == 0 && frame.frame_type == DISCONNECT) {
            EvictedConnection_t dc_evicted[MAX_EVICTIONS_PER_CYCLE];
            uint32_t dc_evicted_count = 0;

            AcquireSRWLockExclusive(&desc->state_lock);
            for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
                KrsArray_t* conns = desc->channel_states[ch].connections;
                if (!conns) continue;
                uint32_t conn_count = krs_array_length(conns);
                uint32_t ci = conn_count;
                while (ci > 0) {
                    ci--;
                    ClientConnection_t* c = KRS_ARRAY_GET(conns, ci, ClientConnection_t);
                    if (!c) continue;
                    if (!krs_network_port_address_equals(&c->remote_address, &msg->remote_address)) continue;
                    if (dc_evicted_count < MAX_EVICTIONS_PER_CYCLE) {
                        dc_evicted[dc_evicted_count].connection_id = c->connection_id;
                        dc_evicted[dc_evicted_count].channel = (Channel_t)ch;
                        dc_evicted_count++;
                    }
                    krs_congestion_destroy(&c->congestion);
                    free(c);
                    krs_array_remove(conns, ci);
                }
            }
            ReleaseSRWLockExclusive(&desc->state_lock);

            if (spm->connection_map) {
                AcquireSRWLockExclusive(&spm->connection_map->lock);
                for (uint32_t e = 0; e < dc_evicted_count; e++) {
                    krs_connection_map_remove(spm->connection_map, dc_evicted[e].connection_id);
                }
                ReleaseSRWLockExclusive(&spm->connection_map->lock);
            }

            InterlockedAdd64(&spm->stat_disconnections, dc_evicted_count);

            if (desc->disconnect_callback) {
                for (uint32_t e = 0; e < dc_evicted_count; e++) {
                    desc->disconnect_callback(dc_evicted[e].connection_id, dc_evicted[e].channel,
                                              desc->disconnect_callback_user_data);
                }
            }

            krs_message_pool_release(spm->message_pool, msg);
            continue;
        } else if (frame.frame_type == MESSAGE_ACK) {
            ChannelState_t* ack_ch_state = &desc->channel_states[frame.channel];
            AcquireSRWLockExclusive(&ack_ch_state->channel_lock);
            AckTracker_t* tracker = ack_ch_state->ack_tracker;
            double rtt_ms = -1.0;
            if (tracker) {
                rtt_ms = krs_ack_tracker_receive_rtt(tracker, frame.packet_id);
            }
            ReleaseSRWLockExclusive(&ack_ch_state->channel_lock);

            InterlockedIncrement64(&spm->stat_ack_received);

            if (rtt_ms >= 0.0 && spm->connection_map) {
                AcquireSRWLockShared(&spm->connection_map->lock);
                uint32_t ack_conn_id = krs_connection_map_get_by_address(spm->connection_map, &msg->remote_address);
                ConnectionMapEntry_t* ack_entry = ack_conn_id ? krs_connection_map_get(spm->connection_map, ack_conn_id) : NULL;
                ClientConnection_t* c = ack_entry ? ack_entry->connection : NULL;
                ReleaseSRWLockShared(&spm->connection_map->lock);

                if (c && c->congestion) {
                    krs_congestion_on_ack(c->congestion, rtt_ms);
                    if (tracker) {
                        AcquireSRWLockExclusive(&ack_ch_state->channel_lock);
                        krs_ack_tracker_set_timeout(tracker, (uint32_t)krs_congestion_get_rto(c->congestion));
                        ReleaseSRWLockExclusive(&ack_ch_state->channel_lock);
                    }
                }
            }

            krs_message_pool_release(spm->message_pool, msg);
            continue;
        } else if (frame.frame_type == SOCKET_SETUP && frame.channel >= 10) {
            AcquireSRWLockExclusive(&desc->state_lock);
            desc->channel_types[frame.channel] = SOCKET_CHANNEL;
            ReleaseSRWLockExclusive(&desc->state_lock);
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        } else if (frame.channel >= 2 && frame.channel <= 9) {
            krs_message_pool_release(spm->message_pool, msg);
            continue;
        } else if (frame.channel >= 10) {
            bool has_fragment_info = (frame.presence_flags & (uint16_t)(1u << META_FLAG_FRAGMENT_INFO)) != 0;

            const uint8_t* callback_data = NULL;
            uint16_t callback_data_length = 0;
            uint8_t* heap_data = NULL;
            bool ack_required = false;

            if (!has_fragment_info) {
                callback_data = frame.body;
                callback_data_length = frame.body_length;
                ack_required = (frame.presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) != 0;
            } else {
                InterlockedIncrement64(&spm->stat_fragments_received);

                ChannelState_t* ch_state = &desc->channel_states[frame.channel];
                AcquireSRWLockExclusive(&ch_state->channel_lock);
                Reassembler_t* reassembler = ch_state->reassembler;
                if (!reassembler) {
                    ch_state->reassembler = krs_reassembler_create();
                    reassembler = ch_state->reassembler;
                }
                if (!reassembler) {
                    ReleaseSRWLockExclusive(&ch_state->channel_lock);
                    krs_message_pool_release(spm->message_pool, msg);
                    continue;
                }
                ReassembleResult_t reassembly = krs_reassembler_feed(reassembler, &frame);
                ReleaseSRWLockExclusive(&ch_state->channel_lock);

                if (!reassembly.complete) {
                    krs_message_pool_release(spm->message_pool, msg);
                    continue;
                }

                InterlockedIncrement64(&spm->stat_fragments_reassembled);

                callback_data = reassembly.data;
                callback_data_length = (uint16_t)reassembly.data_length;
                heap_data = reassembly.data;
                ack_required = reassembly.ack_required;
            }

            if (ack_required) {
                s_send_ack_response(desc, &msg->remote_address, frame.channel, frame.packet_id);
                InterlockedIncrement64(&spm->stat_ack_sent);
            }

            uint32_t conn_id = 0;
            if (spm->connection_map) {
                AcquireSRWLockShared(&spm->connection_map->lock);
                conn_id = krs_connection_map_get_by_address(spm->connection_map, &msg->remote_address);
                ReleaseSRWLockShared(&spm->connection_map->lock);
            }

            ChannelMessageCallback_f cb = desc->channel_callbacks[frame.channel];
            void* ud = desc->channel_callback_user_data[frame.channel];
            if (!cb) {
                cb = desc->port_callback;
                ud = desc->port_callback_user_data;
            }
            ChannelType_e ct = desc->channel_types[frame.channel];

            if (cb) {
                cb(frame.channel, ct, conn_id, callback_data, callback_data_length, ud);
            }

            free(heap_data);
        }

        krs_message_pool_release(spm->message_pool, msg);
    }

    return 0;
}
