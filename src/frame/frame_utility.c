#include "kronos.h"
#include "../internal/frame_body.h"
#include "../internal/frame_metadata.h"

uint16_t krs_frame_calculate_body_length(const uint16_t received_bytes) {
    return received_bytes - KRONOS_FRAME_HEADER_LENGTH;
}

uint16_t krs_frame_body_metadata_get_length(const Frame_t* frame) {
    uint16_t total_length = 0;
    MetadataFlagPosition position = FLAG_COUNT;
    for (int i = 0; i < position; i++) {
        total_length+=KRS_METADATA_FLAG_POSITION_SIZE[i];
    }
    return total_length;
}