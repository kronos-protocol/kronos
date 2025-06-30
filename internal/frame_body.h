#ifndef FRAME_CONTENT_H
#define FRAME_CONTENT_H
#include "kronos.h"
#include <stdint.h>

uint16_t krs_frame_body_metadata_get_length(const Frame* frame);
uint16_t krs_frame_body_body_get_length(const Frame* frame);

#endif //FRAME_CONTENT_H
