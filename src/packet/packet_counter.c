#include "kronos_packet_counter.h"
#include "packet_counter_internal.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>

PacketCounter_t* krs_packet_counter_create(void) {
    PacketCounter_t* counter = malloc(sizeof(PacketCounter_t));
    if (!counter) return NULL;
    memset((void*)counter->counters, 0, sizeof(counter->counters));
    return counter;
}

void krs_packet_counter_destroy(PacketCounter_t** counter) {
    if (!counter || !*counter) return;
    free(*counter);
    *counter = NULL;
}

uint64_t krs_packet_counter_next(PacketCounter_t* counter, Channel_t channel) {
    if (!counter) return 0;
    return (uint64_t)InterlockedIncrement64(&counter->counters[channel]);
}

uint64_t krs_packet_counter_current(const PacketCounter_t* counter, Channel_t channel) {
    if (!counter) return 0;
    return (uint64_t)InterlockedCompareExchange64((volatile LONG64*)&counter->counters[channel], 0, 0);
}

void krs_packet_counter_reset(PacketCounter_t* counter, Channel_t channel) {
    if (!counter) return;
    InterlockedExchange64(&counter->counters[channel], 0);
}
