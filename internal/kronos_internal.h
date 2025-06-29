#ifndef KRONOS_INTERNAL_H
#define KRONOS_INTERNAL_H
#include <stdint.h>

struct Frame {
    uint16_t length; // Given by UDP socket

    // Data in Char Buffer
    // Header
    char protocol_char;       // must be 0x4B ASCII 'K'
    uint8_t protocol_version; // bitmask: major(3bit) + minor(3bit) + patch(2bit)
    uint8_t channel;
    uint8_t frame_type;
    uint16_t presence_flags; // bitmask for flags, each 1bit
    uint64_t packet_id;




    // Body
    uint8_t* data;
};

uint16_t _krs_frame_calculate_data_length(uint16_t received_bytes);

#define KRONOS_FRAME_HEADER_LENGTH 6
#define KRONOS_BUFFER_SIZE 1024

#endif // KRONOS_INTERNAL_H
