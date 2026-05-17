#ifndef FRAME_BUILDER_INTERNAL_H
#define FRAME_BUILDER_INTERNAL_H

#include "kronos.h"
#include "kronos_internal.h"
#include "frame_metadata.h"
#include <stdint.h>

struct FrameBuilder {
    uint8_t channel;
    FrameType_e type;
    uint64_t packet_id;
    uint16_t presence_flags;
    FrameMetadata_t metadata;
    const uint8_t* data;
    uint16_t data_length;
};

#endif //FRAME_BUILDER_INTERNAL_H
