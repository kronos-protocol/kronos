#ifndef PACKET_COUNTER_INTERNAL_H
#define PACKET_COUNTER_INTERNAL_H

#include "kronos_packet_counter.h"

struct PacketCounter {
    uint64_t counters[MAX_CHANNEL_NUMBER + 1];
};

#endif // PACKET_COUNTER_INTERNAL_H
