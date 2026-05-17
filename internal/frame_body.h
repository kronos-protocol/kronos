#ifndef FRAME_CONTENT_H
#define FRAME_CONTENT_H
#include "kronos.h"
#include <stdint.h>

uint16_t krs_frame_metadata_block_length(uint16_t presence_flags);

uint16_t krs_frame_body_metadata_get_length(const Frame_t* frame);

#endif //FRAME_CONTENT_H
