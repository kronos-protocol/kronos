#ifndef KRONOS_INTERNAL_H
#define KRONOS_INTERNAL_H
#include <stdint.h>
#include "../include/kronos.h"

struct Frame {
    uint16_t length; // Given by UDP socket

    // Data in Char Buffer
    // Header
    char protocol_char; // must be 0x4B ASCII 'K'
    uint8_t protocol_major_version; // bitmask: major(3bit) + minor(3bit) + patch(2bit)
    uint8_t frame_type;
    uint16_t presence_flags; // bitmask for flags, each 1bit: frame_id, session_id, sender_id, fragment_info, (Rest Empty for now)

    // Body
    uint8_t data[];
};

#endif //KRONOS_INTERNAL_H
