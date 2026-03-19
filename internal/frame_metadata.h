#ifndef FRAME_METADATA_H
#define FRAME_METADATA_H
#include <stdint.h>

typedef enum {
    META_FLAG_ACK_REQUIRED = 0,
    META_FLAG_FRAGMENT_INFO = 1,
    META_FLAG_ACK_ID       = 2,
    META_FLAG_PRIORITY     = 3,
    META_FLAG_TIMESTAMP    = 4,
    META_FLAG_COUNT
} MetadataFlagPosition_e;

static const uint8_t KRS_METADATA_FLAG_POSITION_SIZE[] = {
    0, 4, 8, 1, 8
};

#endif //FRAME_METADATA_H
