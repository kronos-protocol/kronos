#ifndef KRONOS_INTERNAL_H
#define KRONOS_INTERNAL_H
#include <stdint.h>

typedef struct FrameMetadata FrameMetadata_t;

struct FrameMetadata {
    uint16_t fragment_index;
    uint16_t fragment_total;
    uint64_t ack_id;
    uint8_t priority;
    uint64_t timestamp_ms;
};

struct Frame {
    char protocol_char;
    uint8_t protocol_version;
    uint8_t channel;
    uint8_t frame_type;
    uint16_t presence_flags;
    uint64_t packet_id;

    FrameMetadata_t metadata;

    uint16_t body_length;
    uint8_t* body;
};

#define KRONOS_FRAME_HEADER_LENGTH 14
#define KRONOS_BUFFER_SIZE 1500

#endif // KRONOS_INTERNAL_H
