#include <../include/kronos.h>
#include <../internal/kronos_internal.h>
#include <stdint.h>
#include <string.h>

static Frame s_create_frame(const uint8_t* buffer, const uint16_t received_bytes, uint8_t* frame_data_out) {
    Frame frame;
    frame.length = received_bytes;
    frame.protocol_char = (char)buffer[0];
    frame.protocol_version = buffer[1];
    frame.frame_type = buffer[2];
    frame.presence_flags = ((uint16_t)buffer[3] << 8) | buffer[4];
    frame.data = frame_data_out;
    memcpy(frame_data_out, buffer + KRONOS_FRAME_HEADER_LENGTH, received_bytes - KRONOS_FRAME_HEADER_LENGTH);
    return frame;
}

Frame krs_frame_create(const uint8_t* buffer, uint8_t* stack_data_out, uint16_t received_bytes) {
    return s_create_frame(buffer, received_bytes, stack_data_out);
}