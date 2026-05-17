#ifndef ACK_INTERNAL_H
#define ACK_INTERNAL_H

#include "kronos_array.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct AckEntry AckEntry_t;

struct AckEntry {
    uint64_t packet_id;
    uint8_t* frame_data;
    uint16_t frame_size;
    uint64_t timestamp_ms;
    uint8_t  retry_count;
    uint8_t  acked_after_count;
    uint8_t  channel;
};

struct AckTracker {
    KrsArray_t* pending;
    uint32_t    timeout_ms;
    uint8_t     max_retries;
    bool        fast_retransmit_enabled;
};

#endif // ACK_INTERNAL_H
