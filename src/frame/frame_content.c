#include "kronos.h"
#include "../../internal/frame_body.h"

#include <string.h>

uint16_t krs_frame_get_content(const Frame* frame, uint8_t* out, uint16_t out_data_size) {
    if (frame == NULL) {
        //TODO: handle error with errno
        return 0;
    }

    if (frame->body_length > out_data_size) {
        //TODO: same as above
        return 0;
    }

    int offset = krs_frame_body_metadata_get_length(frame);
    uint16_t new_size = frame->body_length - offset;
    memcpy(out, frame->body + offset, frame->body_length - offset);
    return new_size;
}
