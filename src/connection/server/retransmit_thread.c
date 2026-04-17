#include "kronos_server.h"
#include "kronos_ack.h"
#include "kronos_array.h"
#include "kronos_congestion.h"
#include "kronos_fragment.h"

#include "server_internal.h"
#include "connection_map_internal.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>


#define KRS_RETRANSMIT_INTERVAL_MS 50
#define KRS_HEARTBEAT_TIMEOUT_MS 30000

typedef struct {
    uint8_t data[KRONOS_BUFFER_SIZE];
    uint16_t size;
    PortAddress_t addr;
    UDPSocketRef_t socket;
} PendingRetransmit_t;

typedef struct {
    uint32_t  connection_id;
    Channel_t channel;
} EvictedConnection_t;

#define MAX_EVICTIONS_PER_CYCLE 64
#define MAX_PENDING_RETRANSMITS 64

DWORD WINAPI krs_server_retransmit_thread(LPVOID param) {
    ServerPortManager_t* spm = (ServerPortManager_t*)param;

    while (spm->running) {
        Sleep(KRS_RETRANSMIT_INTERVAL_MS);
        if (!spm->running) break;
        if (!spm->descriptor_list) continue;

        uint32_t desc_count = krs_array_length(spm->descriptor_list);

        PendingRetransmit_t pending[MAX_PENDING_RETRANSMITS];
        uint32_t pending_count = 0;

        for (uint32_t d = 0; d < desc_count; d++) {
            UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, d, UDPSocketDescriptor_t);
            if (!desc) continue;

            for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
                ChannelState_t* ch_state = &desc->channel_states[ch];
                AcquireSRWLockExclusive(&ch_state->channel_lock);
                AckTracker_t* tracker = ch_state->ack_tracker;
                if (!tracker) {
                    ReleaseSRWLockExclusive(&ch_state->channel_lock);
                    continue;
                }

                KrsArray_t* conns = ch_state->connections;
                if (!conns) {
                    ReleaseSRWLockExclusive(&ch_state->channel_lock);
                    continue;
                }

                uint64_t retry_ids[32];
                uint32_t count = krs_ack_tracker_check_timeouts(tracker, retry_ids, 32);
                uint32_t to_check = count < 32 ? count : 32;

                if (to_check > 0) {
                    uint32_t conn_count_cc = krs_array_length(conns);
                    for (uint32_t c = 0; c < conn_count_cc; c++) {
                        ClientConnection_t* conn = KRS_ARRAY_GET(conns, c, ClientConnection_t);
                        if (conn && conn->congestion) {
                            krs_congestion_on_loss(conn->congestion);
                            break;
                        }
                    }
                }

                uint32_t conn_count = krs_array_length(conns);
                for (uint32_t c = 0; c < conn_count && pending_count < MAX_PENDING_RETRANSMITS; c++) {
                    ClientConnection_t* conn = KRS_ARRAY_GET(conns, c, ClientConnection_t);
                    if (!conn) continue;

                    for (uint32_t r = 0; r < to_check && pending_count < MAX_PENDING_RETRANSMITS; r++) {
                        uint16_t frame_size = 0;
                        const uint8_t* frame_data = krs_ack_tracker_get_retry_frame(
                            tracker, retry_ids[r], &frame_size);
                        if (!frame_data || frame_size == 0 || frame_size > KRONOS_BUFFER_SIZE) continue;

                        memcpy(pending[pending_count].data, frame_data, frame_size);
                        pending[pending_count].size = frame_size;
                        pending[pending_count].addr = conn->remote_address;
                        pending[pending_count].socket = desc->udp_socket_ref;
                        pending_count++;
                    }
                }
                ReleaseSRWLockExclusive(&ch_state->channel_lock);
            }
        }

        for (uint32_t i = 0; i < pending_count; i++) {
            WSABUF wsabuf;
            wsabuf.len = pending[i].size;
            wsabuf.buf = (char*)pending[i].data;
            DWORD sent;
            WSASendTo(pending[i].socket, &wsabuf, 1, &sent, 0,
                      (const struct sockaddr*)&pending[i].addr,
                      sizeof(pending[i].addr), NULL, NULL);
            InterlockedIncrement64(&spm->stat_retransmissions);
        }

        for (uint32_t d = 0; d < desc_count; d++) {
            UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, d, UDPSocketDescriptor_t);
            if (!desc) continue;

            EvictedConnection_t evicted[MAX_EVICTIONS_PER_CYCLE];
            uint32_t evicted_count = 0;

            uint64_t now = GetTickCount64();
            AcquireSRWLockExclusive(&desc->state_lock);
            for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
                KrsArray_t* conns = desc->channel_states[ch].connections;
                if (!conns) continue;

                uint32_t i = krs_array_length(conns);
                while (i > 0) {
                    i--;
                    ClientConnection_t* conn = KRS_ARRAY_GET(conns, i, ClientConnection_t);
                    if (!conn) continue;
                    if (now - conn->last_heartbeat_ms < KRS_HEARTBEAT_TIMEOUT_MS) continue;

                    if (evicted_count < MAX_EVICTIONS_PER_CYCLE) {
                        evicted[evicted_count].connection_id = conn->connection_id;
                        evicted[evicted_count].channel = (Channel_t)ch;
                        evicted_count++;
                    }

                    krs_congestion_destroy(&conn->congestion);
                    free(conn);
                    krs_array_remove(conns, i);
                }
            }

            for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
                ChannelState_t* sweep_state = &desc->channel_states[ch];
                AcquireSRWLockExclusive(&sweep_state->channel_lock);
                if (sweep_state->reassembler) {
                    krs_reassembler_sweep_stale(sweep_state->reassembler, 10000);
                }
                ReleaseSRWLockExclusive(&sweep_state->channel_lock);
            }

            ReleaseSRWLockExclusive(&desc->state_lock);

            InterlockedAdd64(&spm->stat_disconnections, evicted_count);

            if (spm->connection_map) {
                AcquireSRWLockExclusive(&spm->connection_map->lock);
                for (uint32_t e = 0; e < evicted_count; e++) {
                    krs_connection_map_remove(spm->connection_map, evicted[e].connection_id);
                }
                ReleaseSRWLockExclusive(&spm->connection_map->lock);
            }

            if (desc->disconnect_callback) {
                for (uint32_t e = 0; e < evicted_count; e++) {
                    desc->disconnect_callback(evicted[e].connection_id, evicted[e].channel,
                                              desc->disconnect_callback_user_data);
                }
            }
            evicted_count = 0;
        }
    }

    return 0;
}
