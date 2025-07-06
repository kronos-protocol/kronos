#include "kronos_math.h"


#include <../include/kronos.h>
#include <../internal/kronos_internal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Frame_t s_create_frame(const uint8_t* buffer, const uint16_t received_bytes, uint8_t* frame_data_out, uint16_t stack_data_out_size) {
    uint16_t data_length = krs_frame_calculate_body_length(received_bytes);
    if (stack_data_out_size < data_length) {
        // TODO: use errno
        const Frame_t invalid = {0};
        return invalid;
    }

    if (buffer == NULL) {
        const Frame_t invalid = {0};
        return invalid;
    }

    if (frame_data_out == NULL) {
        const Frame_t invalid = {0};
        return invalid;
    }

    Frame_t frame;
    frame.body_length = krs_frame_calculate_body_length(received_bytes);
    frame.protocol_char = (char)buffer[0];
    frame.protocol_version = buffer[1];
    frame.channel = buffer[2];
    frame.frame_type = buffer[3];
    frame.presence_flags = krs_math_uint16_from_uint8(buffer[4], buffer[5]);
    frame.packet_id = krs_math_uint64_from_uint8(buffer, 6, received_bytes);
    frame.body_length = data_length;
    frame.body = frame_data_out;
    memcpy(frame_data_out, buffer + KRONOS_FRAME_HEADER_LENGTH, data_length);
    return frame;
}

Frame_t krs_frame_create(const uint8_t* buffer, const uint16_t received_bytes, uint8_t* stack_data_out, uint16_t stack_data_out_size) {
    return s_create_frame(buffer, received_bytes, stack_data_out, stack_data_out_size);
}

Frame_t* krs_frame_create_heap(const uint8_t* buffer, const uint16_t received_bytes) {
    Frame_t* frame = malloc(sizeof(Frame_t));
    uint16_t frame_data_size = krs_frame_calculate_body_length(received_bytes);
    frame->body = malloc(frame_data_size);
    *frame = s_create_frame(buffer, received_bytes, frame->body, frame_data_size);
    return frame;
}


void krs_frame_init(const uint8_t* buffer, const uint16_t received_bytes, Frame_t* out, const uint16_t out_data_size) {
    *out = s_create_frame(buffer, received_bytes, out->body, out_data_size);
}

void krs_frame_destroy(Frame_t** frame) {
    if (*frame != NULL) {
        free((*frame)->body);
        free(*frame);
        *frame = NULL;
    }
}