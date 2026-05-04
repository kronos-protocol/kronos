#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

#include "network_internal.h"
#include "kronos_port_table.h"
#include "kronos_server.h"
#include "kronos_ack.h"
#include "kronos_packet_counter.h"
#include "kronos_fragment.h"
#include "kronos_array.h"
#include "kronos_congestion.h"
#include "iocp_internal.h"
#include "message_queue_internal.h"
#include "message_pool_internal.h"
#include "connection_map_internal.h"

#include <winsock2.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Lock hierarchy (SPEC v20+, post-LOCK_UNIFY):
 *
 *   connection_map->lock  >  desc->state_lock  >  channel_states[ch].channel_lock  >  conn->ack_lock
 *
 * Acquisition order: top-down. Never reverse.
 *
 * Resource ownership:
 *   - desc->channel_states[ch].connections   protected by desc->state_lock
 *   - desc->channel_states[ch].packet_counter protected by channel_lock
 *   - desc->channel_states[ch].reassembler   protected by channel_lock
 *   - desc->port_callback / channel_callbacks[] protected by desc->state_lock
 *   - ClientConnection_t lifetime           refcount-gated; map_lock(shared) is a stronger guarantee
 *   - conn->ack_tracker                      protected by conn->ack_lock
 *   - conn->congestion                       protected by conn->cc_lock (leaf, peer of ack_lock)
 */

typedef struct EvictedConnection EvictedConnection_t;

struct EvictedConnection {
    uint32_t  connection_id;
    Channel_t channel;
};

#define MAX_EVICTIONS_PER_CYCLE 64

typedef struct ChannelState ChannelState_t;

struct ChannelState {
    SRWLOCK          channel_lock;
    PacketCounter_t* packet_counter;
    Reassembler_t*   reassembler;
    KrsArray_t*      connections;
};

struct ClientConnection {
    uint32_t      connection_id;
    PortAddress_t remote_address;
    uint64_t      last_heartbeat_ms;
    CongestionController_t* congestion;
    AckTracker_t* ack_tracker;
    SRWLOCK       ack_lock;
    SRWLOCK       cc_lock;
    volatile LONG refcount;
};

struct UDPSocketDescriptor {
    Port_t                   port;
    UDPSocketRef_t           udp_socket_ref;
    SRWLOCK                  state_lock;
    UDPOverlapped_t*         recv_ctx;
    ChannelType_e            channel_types[MAX_CHANNEL_NUMBER + 1];
    ChannelMessageCallback_f port_callback;
    void*                    port_callback_user_data;
    ChannelMessageCallback_f channel_callbacks[MAX_CHANNEL_NUMBER + 1];
    void*                    channel_callback_user_data[MAX_CHANNEL_NUMBER + 1];
    ChannelState_t           channel_states[MAX_CHANNEL_NUMBER + 1];
    ConnectionLifecycleCallback_f connect_callback;
    void*                         connect_callback_user_data;
    ConnectionLifecycleCallback_f disconnect_callback;
    void*                         disconnect_callback_user_data;
    DeliveryFailureCallback_f     delivery_failure_callback;
    void*                         delivery_failure_callback_user_data;
};

struct ServerPortManager {
    PortTable_t*     port_table;
    KrsArray_t*      descriptor_list;
    Address_t        default_address;
    Channel_t        default_max_channel;
    uint32_t         config_iocp_threads;
    uint32_t         config_handler_threads;
    HANDLE           iocp_handle;
    HANDLE*          iocp_threads;
    uint32_t         iocp_thread_count;
    HANDLE*          handler_threads;
    uint32_t         handler_thread_count;
    MessageQueue_t*  message_queue;
    MessagePool_t*   message_pool;
    ConnectionMap_t* connection_map;
    HANDLE           retransmit_thread;
    volatile bool    running;

    volatile LONG64  stat_messages_received;
    volatile LONG64  stat_messages_sent;
    volatile LONG64  stat_ack_sent;
    volatile LONG64  stat_ack_received;
    volatile LONG64  stat_retransmissions;
    volatile LONG64  stat_connections_total;
    volatile LONG64  stat_disconnections;
    volatile LONG64  stat_fragments_received;
    volatile LONG64  stat_fragments_reassembled;
    volatile LONG64  stat_pool_fallback_mallocs;
};

bool krs_server_port_manager_validate(ServerPortManager_t* server_port_manager);

void krs_server_handle_connection_frame(struct ServerPortManager* spm,
                                        UDPSocketDescriptor_t* descriptor,
                                        const Frame_t* frame,
                                        const PortAddress_t* remote_addr);

void krs_server_handle_heartbeat_frame(UDPSocketDescriptor_t* descriptor,
                                       const Frame_t* frame,
                                       const PortAddress_t* remote_addr);

DWORD WINAPI krs_server_retransmit_thread(LPVOID param);

#endif // SERVER_INTERNAL_H
