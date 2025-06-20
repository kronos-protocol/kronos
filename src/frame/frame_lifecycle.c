#include <../include/kronos.h>
#include <../internal/kronos_internal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Frame s_create_frame(const uint8_t* buffer, const uint16_t received_bytes, uint8_t* frame_data_out, uint16_t stack_data_out_size) {
    uint16_t data_length = _krs_frame_calculate_data_length(received_bytes);
    if (stack_data_out_size > data_length) {
        // TODO: use errno
        const Frame invalid = {0};
        return invalid;
    }

    if (buffer == NULL) {
        const Frame invalid = {0};
        return invalid;
    }

    if (frame_data_out == NULL) {
        const Frame invalid = {0};
        return invalid;
    }

    Frame frame;
    frame.length = received_bytes;
    frame.protocol_char = (char)buffer[0];
    frame.protocol_version = buffer[1];
    frame.frame_type = buffer[2];
    frame.presence_flags = ((uint16_t)buffer[3] << 8) | buffer[4];
    frame.data = frame_data_out;
    memcpy(frame_data_out, buffer + KRONOS_FRAME_HEADER_LENGTH, _krs_frame_calculate_data_length(received_bytes));
    return frame;
}

Frame krs_frame_create(const uint8_t* buffer, const uint16_t received_bytes, uint8_t* stack_data_out, uint16_t stack_data_out_size) {
    return s_create_frame(buffer, received_bytes, stack_data_out, stack_data_out_size);
}

Frame* krs_frame_create_heap(const uint8_t* buffer, const uint16_t received_bytes) {
    Frame* frame = malloc(sizeof(Frame));
    uint16_t frame_data_size = _krs_frame_calculate_data_length(received_bytes);
    uint8_t* frame_data = malloc(frame_data_size);
    *frame = s_create_frame(buffer, received_bytes, frame_data, frame_data_size);
    return frame;
}


void krs_frame_init(const uint8_t* buffer, const uint16_t received_bytes, Frame* out, const uint16_t out_data_size) {
    *out = s_create_frame(buffer, received_bytes, out->data, out_data_size);
}