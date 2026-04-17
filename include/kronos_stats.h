#ifndef KRONOS_STATS_H
#define KRONOS_STATS_H

#include <stdint.h>

/** @brief Snapshot of server runtime statistics. */
typedef struct ServerStats ServerStats_t;

struct ServerStats {
    uint64_t messages_received;
    uint64_t messages_sent;
    uint64_t ack_sent;
    uint64_t ack_received;
    uint64_t retransmissions;
    uint64_t connections_active;
    uint64_t connections_total;
    uint64_t disconnections;
    uint64_t fragments_received;
    uint64_t fragments_reassembled;
    uint64_t pool_acquires;
    uint64_t pool_fallback_mallocs;
};

#endif // KRONOS_STATS_H
