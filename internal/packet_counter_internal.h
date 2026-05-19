#ifndef PACKET_COUNTER_INTERNAL_H
#define PACKET_COUNTER_INTERNAL_H

#include "kronos_packet_counter.h"

#include <winsock2.h>

struct PacketCounter {
    volatile LONG64 counters[MAX_CHANNEL_NUMBER + 1];
};

#endif // PACKET_COUNTER_INTERNAL_H
