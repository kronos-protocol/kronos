#include "kronos_server.h"
#include "kronos_ack.h"
#include "kronos_array.h"
#include "kronos_congestion.h"
#include "kronos_fragment.h"

#include "server_internal.h"
#include "connection_map_internal.h"
#include "net_send_internal.h"

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

#define MAX_PENDING_RETRANSMITS 64

typedef struct {
    uint32_t  connection_id;
    Channel_t channel;
    uint64_t  packet_id;
    DeliveryFailureCallback_f callback;
    void*     user_data;
} PendingDeliveryFailure_t;

#define MAX_PENDING_FAILURES 32

static uint32_t s_collect_retransmits(ServerPortManager_t* spm,
                                      UDPSocketDescriptor_t* desc,
                                      PendingRetransmit_t* pending,
                                      uint32_t pending_capacity,
                                      uint32_t current_count,
                                      PendingDeliveryFailure_t* failures,
                                      uint32_t failures_capacity,
                                      uint32_t* failures_count) {
    uint32_t pending_count = current_count;

    if (!spm->connection_map) return pending_count;

    AcquireSRWLockShared(&spm->connection_map->lock);

    for (uint32_t slot = 0; slot < spm->connection_map->capacity; slot++) {
        ConnectionMapEntry_t* entry = &spm->connection_map->entries[slot];
        if (!entry->occupied || entry->deleted) continue;
        if (entry->descriptor != desc) continue;

        ClientConnection_t* conn = entry->connection;
        if (!conn) continue;

        AcquireSRWLockExclusive(&conn->ack_lock);
        AckTracker_t* tracker = conn->ack_tracker;
        if (!tracker) {
            ReleaseSRWLockExclusive(&conn->ack_lock);
            continue;
        }

        uint64_t retry_ids[32];
        uint8_t  retry_channels[32];
        AckEntry_t* retry_entries[32];
        bool     retry_was_fast[32];
        uint64_t dropped_ids[32];
        uint8_t  dropped_channels[32];
        uint32_t dropped_count = 0;
        uint32_t retry_count = krs_ack_tracker_check_timeouts(tracker,
                                                              retry_ids, retry_channels, retry_entries,
                                                              retry_was_fast, 32,
                                                              dropped_ids, dropped_channels, 32,
                                                              &dropped_count);
        uint32_t to_check = retry_count < 32 ? retry_count : 32;

        bool any_timeout_retry = false;
        bool any_fast_retry = false;
        for (uint32_t r = 0; r < to_check && pending_count < pending_capacity; r++) {
            uint16_t frame_size = 0;
            const uint8_t* frame_data = krs_ack_tracker_get_retry_frame_for_entry(
                retry_entries[r], &frame_size);
            if (!frame_data || frame_size == 0 || frame_size > KRONOS_BUFFER_SIZE) continue;

            memcpy(pending[pending_count].data, frame_data, frame_size);
            pending[pending_count].size = frame_size;
            pending[pending_count].addr = conn->remote_address;
            pending[pending_count].socket = desc->udp_socket_ref;
            pending_count++;

            if (retry_was_fast[r]) any_fast_retry = true;
            else                   any_timeout_retry = true;
        }

        ReleaseSRWLockExclusive(&conn->ack_lock);

        if (conn->congestion && (any_fast_retry || any_timeout_retry)) {
            AcquireSRWLockExclusive(&conn->cc_lock);
            if (any_timeout_retry) {
                krs_congestion_on_timeout_loss(conn->congestion);
            } else {
                krs_congestion_on_fast_retransmit_loss(conn->congestion);
            }
            ReleaseSRWLockExclusive(&conn->cc_lock);
        }

        if (dropped_count > 0 && desc->delivery_failure_callback) {
            for (uint32_t d = 0; d < dropped_count && *failures_count < failures_capacity; d++) {
                failures[*failures_count].connection_id = conn->connection_id;
                failures[*failures_count].channel = dropped_channels[d];
                failures[*failures_count].packet_id = dropped_ids[d];
                failures[*failures_count].callback = desc->delivery_failure_callback;
                failures[*failures_count].user_data = desc->delivery_failure_callback_user_data;
                (*failures_count)++;
            }
        }
    }

    ReleaseSRWLockShared(&spm->connection_map->lock);

    return pending_count;
}

static void s_flush_retransmits(ServerPortManager_t* spm,
                                const PendingRetransmit_t* pending,
                                uint32_t pending_count) {
    for (uint32_t i = 0; i < pending_count; i++) {
        krs_net_send_frame(pending[i].socket, &pending[i].addr,
                           pending[i].data, pending[i].size);
        InterlockedIncrement64(&spm->stat_retransmissions);
    }
}

static uint32_t s_evict_stale_connections(UDPSocketDescriptor_t* desc,
                                          ClientConnection_t** orphaned,
                                          uint32_t orphan_capacity) {
    uint32_t orphan_count = 0;
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

            bool already_tracked = false;
            for (uint32_t u = 0; u < orphan_count; u++) {
                if (orphaned[u] == conn) { already_tracked = true; break; }
            }
            if (already_tracked) {
                krs_array_remove(conns, i);
                continue;
            }

            bool stale = (now - conn->last_heartbeat_ms >= KRS_HEARTBEAT_TIMEOUT_MS);
            if (stale) {
                if (orphan_count < orphan_capacity) {
                    orphaned[orphan_count++] = conn;
                }
                krs_array_remove(conns, i);
            }
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

    return orphan_count;
}

static void s_retransmit_tick(ServerPortManager_t* spm) {
    if (!spm->descriptor_list) return;

    uint32_t desc_count = krs_array_length(spm->descriptor_list);

    PendingRetransmit_t pending[MAX_PENDING_RETRANSMITS];
    uint32_t pending_count = 0;

    PendingDeliveryFailure_t failures[MAX_PENDING_FAILURES];
    uint32_t failures_count = 0;

    for (uint32_t d = 0; d < desc_count; d++) {
        UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, d, UDPSocketDescriptor_t);
        if (!desc) continue;
        pending_count = s_collect_retransmits(spm, desc, pending, MAX_PENDING_RETRANSMITS, pending_count,
                                              failures, MAX_PENDING_FAILURES, &failures_count);
    }

    s_flush_retransmits(spm, pending, pending_count);

    for (uint32_t f = 0; f < failures_count; f++) {
        failures[f].callback(failures[f].connection_id, failures[f].channel,
                             failures[f].packet_id, failures[f].user_data);
    }

    for (uint32_t d = 0; d < desc_count; d++) {
        UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, d, UDPSocketDescriptor_t);
        if (!desc) continue;

        ClientConnection_t* orphans[MAX_EVICTIONS_PER_CYCLE];
        uint32_t orphan_count = s_evict_stale_connections(desc, orphans, MAX_EVICTIONS_PER_CYCLE);
        krs_server_finalize_orphan_evictions(spm, desc, orphans, orphan_count);
    }
}

DWORD WINAPI krs_server_retransmit_thread(LPVOID param) {
    ServerPortManager_t* spm = (ServerPortManager_t*)param;

    while (spm->running) {
        Sleep(KRS_RETRANSMIT_INTERVAL_MS);
        if (!spm->running) break;
        s_retransmit_tick(spm);
    }

    return 0;
}
