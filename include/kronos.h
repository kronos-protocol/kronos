#ifndef KRONOS_H
#define KRONOS_H
#include "../internal/kronos_internal.h"
#include <stdint.h>

typedef struct Frame Frame;
typedef enum FrameType FrameType;

Frame krs_frame_create(const uint8_t* buffer, uint16_t received_bytes, uint8_t* stack_data_out, uint16_t stack_data_out_size);
Frame* krs_frame_create_heap(const uint8_t* buffer, uint16_t received_bytes);
void krs_frame_init(const uint8_t* buffer, uint16_t received_bytes, Frame* out, uint16_t out_data_size);

void krs_frame_get_content(const Frame* frame, uint8_t* out, uint16_t out_data_size);


void krs_frame_destroy(Frame** frame);

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
