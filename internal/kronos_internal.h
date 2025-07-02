#ifndef KRONOS_INTERNAL_H
#define KRONOS_INTERNAL_H
#include <stdint.h>

struct Frame {
    // Data in Char Buffer
    // Header
    char protocol_char;       // must be 0x4B ASCII 'K'
    uint8_t protocol_version; // bitmask: major(3bit) + minor(3bit) + patch(2bit)
    uint8_t channel;
    uint8_t frame_type;
    uint16_t presence_flags; // bitmask for flags, each 1bit
    uint64_t packet_id;

    // Body
    uint16_t body_length;
    uint8_t* body; // Contains Metadata from presence_flags and the actual data that is transferred
};

#define KRONOS_FRAME_HEADER_LENGTH 14
#define KRONOS_BUFFER_SIZE 1024

#endif // KRONOS_INTERNAL_H
