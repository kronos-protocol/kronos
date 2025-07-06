#ifndef KRONOS_H
#define KRONOS_H
#include "../internal/kronos_internal.h"
#include <stdint.h>

typedef struct Frame Frame_t;
typedef enum FrameType FrameType_t;

Frame_t krs_frame_create(const uint8_t* buffer, uint16_t received_bytes, uint8_t* stack_data_out, uint16_t stack_data_out_size);
Frame_t* krs_frame_create_heap(const uint8_t* buffer, uint16_t received_bytes);
void krs_frame_init(const uint8_t* buffer, uint16_t received_bytes, Frame_t* out, uint16_t out_data_size);

uint16_t krs_frame_calculate_body_length(uint16_t received_bytes);

uint16_t krs_frame_get_content(const Frame_t* frame, uint8_t* out, uint16_t out_data_size); // returns size that was filled with content
void krs_frame_destroy(Frame_t** frame);

enum FrameType {
    // General
    MESSAGE_ACK = 0,
    BASIC_MESSAGE = 1,

    // Client-Only
    CONNECTION = 10,
    HEARTBEAT = 11,
    SOCKET_SETUP = 12,

    // Server-Only
    SOCKET_ACK = 22,
};

#endif // KRONOS_H
