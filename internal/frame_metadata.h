#ifndef FRAME_METADATA_H
#define FRAME_METADATA_H
#include <stdint.h>

/**
 * @brief Bit positions of metadata-presence flags inside a frame header.
 *
 * Each value is the bit index in the frame's 16-bit `presence_flags` field
 * (offset 4–5 in the header), counted from the most-significant bit.
 *
 * @note Builder coverage is incomplete (see SPEC.md Known Issue #12).
 *       Only META_FLAG_ACK_REQUIRED (presence-only) and META_FLAG_FRAGMENT_INFO
 *       (written inline by krs_fragment_split) are correctly serialised end
 *       to end. META_FLAG_ACK_ID, META_FLAG_PRIORITY, and META_FLAG_TIMESTAMP
 *       are reserved positions; the frame builder does not currently emit
 *       their payload bytes. Setting these flags via krs_frame_builder_set_flag
 *       produces a wire-incorrect frame.
 */
typedef enum {
    /** @brief Receiver must reply with a MESSAGE_ACK frame carrying the same packet_id. Payload size: 0. */
    META_FLAG_ACK_REQUIRED = 0,

    /** @brief Frame is one piece of a larger message. Payload size: 4 bytes (index:2 + total:2). */
    META_FLAG_FRAGMENT_INFO = 1,

    /** @brief Reserved. Payload size: 8 bytes. Not currently written by the builder. */
    META_FLAG_ACK_ID       = 2,

    /** @brief Reserved. Payload size: 1 byte. Not currently written by the builder. */
    META_FLAG_PRIORITY     = 3,

    /** @brief Reserved. Payload size: 8 bytes. Not currently written by the builder. */
    META_FLAG_TIMESTAMP    = 4,

    /** @brief Sentinel; equal to the number of defined flag positions. */
    META_FLAG_COUNT
} MetadataFlagPosition_e;

/**
 * @brief Per-flag payload size in bytes, indexed by MetadataFlagPosition_e.
 *
 * Used by the frame body parser to compute the metadata-block length given
 * a presence_flags bitmask. Values must stay in lockstep with the flag
 * enumeration above.
 */
static const uint8_t KRS_METADATA_FLAG_POSITION_SIZE[] = {
    0, 4, 8, 1, 8
};

#endif //FRAME_METADATA_H
