#ifndef KRONOS_FRAGMENT_H
#define KRONOS_FRAGMENT_H

#include "kronos.h"
#include "kronos_error.h"

#include <stdint.h>
#include <stdbool.h>

/** @brief Default maximum transmission unit for Kronos frames. */
#define KRS_DEFAULT_MTU 1400

/** @brief Maximum payload bytes per fragment when fragmentation is active. */
#define KRS_MAX_PAYLOAD_PER_FRAGMENT (KRS_DEFAULT_MTU - KRONOS_FRAME_HEADER_LENGTH - 4)

/**
 * @brief Result of a fragmentation operation.
 *
 * On success, fragments and fragment_sizes are heap-allocated arrays of fragment_count elements.
 * Call krs_fragment_result_destroy() to free them.
 */
typedef struct FragmentResult FragmentResult_t;

/** @brief Opaque reassembler that accumulates fragments until a full payload is ready. */
typedef struct Reassembler Reassembler_t;

/**
 * @brief Result of feeding a fragment to the reassembler.
 *
 * When complete is true, data is a heap-allocated buffer owned by the caller
 * and must be freed with free(). When complete is false, data is NULL.
 */
typedef struct ReassembleResult ReassembleResult_t;

struct FragmentResult {
    KronosResult_b base;
    uint8_t** fragments;
    uint16_t* fragment_sizes;
    uint16_t fragment_count;
    uint8_t* _data_pool;
};

struct ReassembleResult {
    KronosResult_b base;
    bool complete;
    uint8_t* data;
    uint32_t data_length;
    bool ack_required;
};

/**
 * @brief Splits a payload into one or more serialized Kronos frames bounded by mtu.
 *
 * If the payload fits within a single frame, returns one frame without the
 * FRAGMENT_INFO flag. Otherwise splits across multiple frames, each carrying a
 * 4-byte fragment header and the META_FLAG_FRAGMENT_INFO flag. Any bits set in
 * additional_flags are ORed into each frame's presence_flags field.
 *
 * @param channel           Channel number (0–255).
 * @param type              Frame type identifier.
 * @param packet_id         Packet ID used to correlate all fragments.
 * @param data              Payload bytes to fragment.
 * @param data_length       Number of payload bytes.
 * @param mtu               Maximum frame size in bytes.
 * @param additional_flags  Extra presence flag bits to set on every output frame
 *                          (e.g. 1u << META_FLAG_ACK_REQUIRED). Pass 0 for none.
 * @return FragmentResult_t with allocated fragment buffers, or a failed result on error.
 *
 * @retval KRS_SUCCESS                 Fragmentation succeeded.
 * @retval KRS_ERR_INVALID_PARAMETER   mtu is too small to hold a header and at least one byte.
 * @retval KRS_ERR_MEMORY_ALLOCATION   Buffer allocation failed.
 */
FragmentResult_t krs_fragment_split(uint8_t channel, FrameType_e type, uint64_t packet_id,
                                    const uint8_t* data, uint32_t data_length, uint16_t mtu,
                                    uint16_t additional_flags);

/**
 * @brief Frees all heap-allocated buffers inside a FragmentResult_t.
 *
 * @param result  Pointer to the result to clean up.
 */
void krs_fragment_result_destroy(FragmentResult_t* result);

/**
 * @brief Creates a new reassembler to accumulate incoming fragments.
 *
 * @return Pointer to the new Reassembler_t, or NULL on allocation failure.
 */
Reassembler_t* krs_reassembler_create(void);

/**
 * @brief Destroys a reassembler and frees all buffered fragment sessions.
 *
 * @param reassembler  Pointer to the reassembler pointer; set to NULL on return.
 */
void krs_reassembler_destroy(Reassembler_t** reassembler);

/**
 * @brief Feeds a parsed frame into the reassembler.
 *
 * If the frame does not carry META_FLAG_FRAGMENT_INFO, returns a complete result
 * immediately with a copy of the frame body. If the frame is a fragment, buffers
 * the payload and returns complete=true with the fully reassembled data only when
 * all fragments for the packet_id have been received. Duplicate fragments are ignored.
 *
 * @param reassembler  The reassembler to feed.
 * @param fragment     Parsed Kronos frame (may or may not be a fragment).
 * @return ReassembleResult_t. When complete, caller owns result.data and must free() it.
 *
 * @retval KRS_SUCCESS                       Complete or partial reassembly succeeded.
 * @retval KRS_ERR_NULL_POINTER              reassembler or fragment is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER         Fragment body too small for fragment header,
 *                                           or invalid fragment index/total.
 * @retval KRS_ERR_FRAGMENT_PAYLOAD_OVERSIZED Fragment payload exceeds KRS_MAX_PAYLOAD_PER_FRAGMENT.
 * @retval KRS_ERR_MEMORY_ALLOCATION         Buffer or session allocation failed.
 */
ReassembleResult_t krs_reassembler_feed(Reassembler_t* reassembler, const Frame_t* fragment);

/**
 * @brief Removes fragment sessions older than the specified timeout.
 *
 * Iterates all active sessions and destroys any whose creation time
 * is older than timeout_ms milliseconds ago. Call periodically to
 * prevent memory leaks from incomplete fragment sets.
 *
 * @param reassembler  The reassembler to sweep.
 * @param timeout_ms   Maximum age of a session in milliseconds.
 * @return Number of sessions removed.
 */
uint32_t krs_reassembler_sweep_stale(Reassembler_t* reassembler, uint32_t timeout_ms);

#endif // KRONOS_FRAGMENT_H
