#include "kronos.h"
#include "kronos_math.h"

#include "kronos_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static Frame_t s_create_frame(const uint8_t* buffer, const uint16_t received_bytes, uint8_t* frame_data_out, uint16_t stack_data_out_size) {
    const Frame_t invalid = {0};

    if (buffer == NULL || frame_data_out == NULL) return invalid;
    if (received_bytes < KRONOS_FRAME_HEADER_LENGTH) return invalid;
    if ((uint8_t)buffer[0] != 0x4B) return invalid;
    if (((buffer[1] >> 5) & 0x07u) != KRONOS_VERSION_MAJOR) return invalid;

    uint16_t data_length = krs_frame_calculate_body_length(received_bytes);
    if (stack_data_out_size < data_length) return invalid;

    Frame_t frame;
    frame.protocol_char = buffer[0];
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

FrameCreate_r krs_frame_create_s(const uint8_t* buffer, uint16_t received_bytes,
                                 uint8_t* stack_data_out, uint16_t stack_data_out_size) {
    FrameCreate_r result = {0};

    if (!buffer || !stack_data_out) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "buffer or stack_data_out is NULL");
        return result;
    }
    if (received_bytes < KRONOS_FRAME_HEADER_LENGTH) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_FRAME_INVALID_HEADER, "received_bytes too small");
        return result;
    }
    if (buffer[0] != 0x4B) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_FRAME_INVALID_PROTOCOL, "first byte is not 0x4B");
        return result;
    }
    if (((buffer[1] >> 5) & 0x07u) != KRONOS_VERSION_MAJOR) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_FRAME_UNSUPPORTED_VERSION, "frame major version does not match build");
        return result;
    }

    uint16_t data_length = krs_frame_calculate_body_length(received_bytes);
    if (stack_data_out_size < data_length) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_BUFFER_TOO_SMALL, "stack_data_out_size too small");
        return result;
    }

    result.frame = s_create_frame(buffer, received_bytes, stack_data_out, stack_data_out_size);
    result.base = krs_lib_error_result_base_suc();
    return result;
}

Frame_t* krs_frame_create_heap(const uint8_t* buffer, const uint16_t received_bytes) {
    if (!buffer || received_bytes < KRONOS_FRAME_HEADER_LENGTH) return NULL;

    Frame_t* frame = malloc(sizeof(Frame_t));
    if (!frame) return NULL;

    uint16_t body_size = krs_frame_calculate_body_length(received_bytes);
    uint8_t* body = malloc(body_size);
    if (!body) {
        free(frame);
        return NULL;
    }

    *frame = s_create_frame(buffer, received_bytes, body, body_size);
    if (frame->protocol_char == 0) {
        free(body);
        free(frame);
        return NULL;
    }

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
