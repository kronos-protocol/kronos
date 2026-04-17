#ifndef FRAGMENT_INTERNAL_H
#define FRAGMENT_INTERNAL_H

#include "kronos_array.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct FragmentSession FragmentSession_t;

struct FragmentSession {
    uint64_t  packet_id;
    uint16_t  total;
    uint16_t  received;
    uint8_t*  buffer;
    uint16_t  max_piece_size;
    uint16_t* piece_sizes;
    bool*     piece_received;
    bool      ack_required;
    uint64_t  created_ms;
};

struct Reassembler {
    KrsArray_t* sessions;
};

#endif // FRAGMENT_INTERNAL_H
