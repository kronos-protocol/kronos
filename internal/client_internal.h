#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H

#include "kronos_client.h"
#include "kronos_network.h"
#include "kronos_packet_counter.h"
#include "kronos_fragment.h"
#include "kronos_server.h"
#include "kronos_ack.h"
#include "kronos_congestion.h"

#include <stdbool.h>
#include <stdint.h>
// Windows specific for now
#include <winsock2.h>

struct ServerConnection {
    UDPSocketRef_t           socket;
    PortAddress_t            server_address;
    CRITICAL_SECTION         state_lock;
    uint32_t                 connection_id;
    bool                     connected;
    PacketCounter_t*         packet_counter;
    HANDLE                   recv_thread;
    volatile bool            running;
    ChannelMessageCallback_f callback;
    void*                    callback_user_data;
    Reassembler_t*            reassembler;
    AckTracker_t*             ack_tracker;
    CongestionController_t*   congestion;
    uint64_t                  last_heartbeat_sent_ms;
    DeliveryFailureCallback_f delivery_failure_callback;
    void*                     delivery_failure_callback_user_data;
};

#endif // CLIENT_INTERNAL_H
