#include "kronos.h"

#include <string.h>

uint16_t krs_frame_get_content(const Frame_t* frame, uint8_t* out, uint16_t out_data_size) {
    if (frame == NULL || out == NULL) return 0;
    if (frame->body_length > out_data_size) return 0;

    if (frame->body_length > 0) {
        memcpy(out, frame->body, frame->body_length);
    }
    return frame->body_length;
}
