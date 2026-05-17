#ifndef FRAME_METADATA_H
#define FRAME_METADATA_H
#include <stdint.h>

/**
 * @brief Bit positions of metadata-presence flags inside a frame header.
 *
 * Each value is the bit index in the frame's 16-bit `presence_flags` field
 * (offset 4-5 in the header), counted from the most-significant bit.
 *
 * Metadata payloads, when their flag is set, are serialized into the body
 * in bit-position order, before the application body. Sizes per flag are
 * given by KRS_METADATA_FLAG_POSITION_SIZE.
 *
 * Wire layout when both FRAGMENT_INFO and ACK_ID are set, for example:
 *   [14-17]  fragment index(2) + total(2)
 *   [18-25]  ack_id(8)
 *   [26..]   application body
 */
typedef enum {
    /** @brief Receiver must reply with a MESSAGE_ACK frame carrying the same packet_id. Payload size: 0. */
    META_FLAG_ACK_REQUIRED = 0,

    /** @brief Frame is one piece of a larger message. Payload size: 4 bytes (index:2 + total:2). */
    META_FLAG_FRAGMENT_INFO = 1,

    /** @brief Inline acknowledgement of a packet ID. Payload size: 8 bytes. */
    META_FLAG_ACK_ID       = 2,

    /** @brief Application-defined priority value. Payload size: 1 byte. */
    META_FLAG_PRIORITY     = 3,

    /** @brief Sender-supplied timestamp (interpretation is application-defined). Payload size: 8 bytes. */
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
