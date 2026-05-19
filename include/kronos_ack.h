#ifndef KRONOS_ACK_H
#define KRONOS_ACK_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Opaque selective ACK tracker. */
typedef struct AckTracker AckTracker_t;

/** @brief Opaque ACK pending-entry handle returned by krs_ack_tracker_check_timeouts. */
typedef struct AckEntry AckEntry_t;

/**
 * @brief Creates a new ACK tracker.
 *
 * @param timeout_ms   Milliseconds before an unacknowledged packet is considered timed out.
 * @param max_retries  Maximum number of retries before a packet is dropped.
 * @return Pointer to the new AckTracker_t, or NULL on allocation failure.
 */
AckTracker_t* krs_ack_tracker_create(uint32_t timeout_ms, uint8_t max_retries);

/**
 * @brief Destroys an ACK tracker and frees all pending entries.
 *
 * @param tracker  Pointer to the tracker pointer; set to NULL on return.
 */
void krs_ack_tracker_destroy(AckTracker_t** tracker);

/**
 * @brief Registers a packet as pending acknowledgement.
 *
 * Copies frame_data internally. No-op if tracker is NULL or allocation fails.
 *
 * @param tracker     The tracker to register the packet with.
 * @param packet_id   Unique ID of the sent packet.
 * @param channel     Channel the packet was sent on (preserved for retry and drop reporting).
 * @param frame_data  Raw frame bytes to retain for potential retransmission.
 * @param frame_size  Number of bytes in frame_data.
 */
void krs_ack_tracker_expect(AckTracker_t* tracker, uint64_t packet_id, uint8_t channel,
                            const uint8_t* frame_data, uint16_t frame_size);

/**
 * @brief Records that a packet was acknowledged, removing it from tracking.
 *
 * No-op if tracker is NULL or packet_id is not found.
 *
 * @param tracker          The tracker to update.
 * @param acked_packet_id  ID of the packet that was acknowledged.
 * @param channel          Channel the ACK was received on. Used to scope fast-retransmit
 *                         observation: only pending entries on the same channel have
 *                         their `acked_after_count` incremented. Must match the channel
 *                         of the corresponding original send.
 */
void krs_ack_tracker_receive(AckTracker_t* tracker, uint64_t acked_packet_id, uint8_t channel);

/**
 * @brief Records that a packet was acknowledged and returns the RTT sample.
 *
 * Removes all entries matching acked_packet_id and returns the RTT
 * (in milliseconds) of the first matching entry. Returns -1.0 if
 * the tracker is NULL or the packet_id is not found.
 *
 * @param tracker          The tracker to update.
 * @param acked_packet_id  ID of the packet that was acknowledged.
 * @param channel          Channel the ACK was received on. Used to scope fast-retransmit
 *                         observation: only pending entries on the same channel have
 *                         their `acked_after_count` incremented. Must match the channel
 *                         of the corresponding original send.
 * @return RTT in milliseconds, or -1.0 if not found.
 */
double krs_ack_tracker_receive_rtt(AckTracker_t* tracker, uint64_t acked_packet_id, uint8_t channel);

/**
 * @brief Checks for timed-out packets, returns retransmission candidates,
 *        and reports permanently-dropped packets, with per-entry channel
 *        and detection-mode attribution.
 *
 * Packets that have exceeded max_retries are dropped; their packet IDs are written
 * to dropped_ids_out (if non-NULL) up to dropped_capacity entries, and their
 * channels are written to dropped_channels_out (if non-NULL) at the same index.
 * For each remaining timed-out packet, its ID is written to retry_ids_out
 * (up to out_capacity entries), its channel to retry_channels_out (if non-NULL),
 * its retry count is incremented, and its timeout timer is reset.
 *
 * The detection mode for each retry is reported in retry_was_fast_out (if
 * non-NULL): true means the retry was triggered by fast-retransmit
 * (KRS_FAST_RETRANSMIT_THRESHOLD later packets ACKed); false means it was
 * triggered by the elapsed-time / RTO timer. When both conditions fire
 * simultaneously the entry is reported as fast (the FR signal carries strictly
 * more information about network state — at least three later packets did
 * make it through). Callers route fast vs. timeout retries to the matching
 * krs_congestion_on_*_loss entry point.
 *
 * Pass NULL for either channel buffer to skip channel reporting on that side.
 *
 * @param tracker               The tracker to check.
 * @param retry_ids_out         Output buffer for packet IDs that need retransmission.
 * @param retry_channels_out    Optional output buffer for channels of those retries.
 * @param retry_entries_out     Optional output buffer for retry-candidate entry handles
 *                              (may be NULL). Each handle is valid only until the next
 *                              call that mutates the tracker (another check_timeouts,
 *                              expect, receive, or destroy). Use
 *                              krs_ack_tracker_get_retry_frame_for_entry() to access
 *                              frame data without re-scanning the pending list.
 * @param retry_was_fast_out    Optional output buffer; entry r is true if retry r was
 *                              triggered by fast-retransmit detection, false if by
 *                              elapsed-timeout. Same indexing as retry_ids_out.
 * @param out_capacity          Capacity of retry_ids_out (and retry_channels_out /
 *                              retry_entries_out / retry_was_fast_out if non-NULL).
 * @param dropped_ids_out       Optional output buffer for permanently-dropped packet IDs.
 * @param dropped_channels_out  Optional output buffer for channels of those drops.
 * @param dropped_capacity      Capacity of dropped_ids_out / dropped_channels_out.
 * @param dropped_count_out     Optional pointer; set to the number of IDs written
 *                              to dropped_ids_out (clamped to dropped_capacity).
 *                              Pass NULL to skip. Set to 0 if dropped_ids_out is NULL.
 * @return Total number of packets that timed out and were scheduled for retry
 *         (not counting dropped packets; may exceed out_capacity).
 */
uint32_t krs_ack_tracker_check_timeouts(AckTracker_t* tracker,
                                        uint64_t* retry_ids_out,
                                        uint8_t* retry_channels_out,
                                        AckEntry_t** retry_entries_out,
                                        bool* retry_was_fast_out,
                                        uint32_t out_capacity,
                                        uint64_t* dropped_ids_out,
                                        uint8_t* dropped_channels_out,
                                        uint32_t dropped_capacity,
                                        uint32_t* dropped_count_out);

/**
 * @brief Retrieves the stored frame data for a pending packet, for retransmission.
 *
 * Returns the internally stored frame bytes for the given packet_id without
 * removing it from tracking. Returns NULL if the tracker is NULL, the packet_id
 * is not found, or the stored frame data is empty.
 *
 * @param tracker     The ACK tracker to query.
 * @param packet_id   The packet ID to look up.
 * @param frame_size_out  Output: number of bytes in the returned frame. Set to 0 on failure.
 * @return Pointer to the internally stored frame bytes (valid until the entry is
 *         removed by krs_ack_tracker_receive or max-retry drop), or NULL.
 */
const uint8_t* krs_ack_tracker_get_retry_frame(const AckTracker_t* tracker,
                                                uint64_t packet_id,
                                                uint16_t* frame_size_out);

/**
 * @brief Returns the stored retry frame data for a given AckEntry handle.
 *
 * Faster than krs_ack_tracker_get_retry_frame: skips the linear scan over
 * the pending list. The handle must come from a recent
 * krs_ack_tracker_check_timeouts call on the same tracker, and the tracker
 * must not have been mutated (expect / receive / receive_rtt / another
 * check_timeouts / destroy) since that call.
 *
 * @param entry           Opaque entry handle obtained from check_timeouts.
 * @param frame_size_out  Output for the frame size in bytes (may be NULL).
 *                        Set to 0 on failure.
 * @return Pointer to the internally stored frame bytes, or NULL if entry is
 *         NULL or has no stored data. Validity matches the @p entry handle —
 *         do not retain past the next tracker mutation.
 */
const uint8_t* krs_ack_tracker_get_retry_frame_for_entry(const AckEntry_t* entry,
                                                          uint16_t* frame_size_out);

/**
 * @brief Updates the base timeout used for detecting timed-out packets.
 *
 * Allows the congestion controller to feed its computed RTO back to the
 * tracker for adaptive retransmission timing. The effective per-retry
 * timeout is `timeout_ms << min(retry_count, 6)`, clamped to 60s — TCP-style
 * exponential backoff. The first retry waits `timeout_ms`; the second waits
 * `2 * timeout_ms`; etc. Fast-retransmit detection is unaffected by backoff.
 *
 * @param tracker     The ACK tracker.
 * @param timeout_ms  Base timeout in milliseconds. Clamped to minimum 50ms.
 */
void krs_ack_tracker_set_timeout(AckTracker_t* tracker, uint32_t timeout_ms);

/**
 * @brief Toggles fast-retransmit detection for this tracker.
 *
 * When enabled (the default), pending packets are flagged for immediate
 * retransmission as soon as KRS_FAST_RETRANSMIT_THRESHOLD (3) later packets
 * have been acknowledged. Detection happens in krs_ack_tracker_check_timeouts
 * and is therefore subject to the retransmit thread's tick interval (~50ms).
 *
 * When disabled, only the timeout-based retransmission path is active —
 * useful for A/B benchmarks comparing fast vs. slow detection.
 *
 * @param tracker  The ACK tracker.
 * @param enabled  true to enable fast retransmit (default), false to disable.
 */
void krs_ack_tracker_set_fast_retransmit_enabled(AckTracker_t* tracker, bool enabled);

/**
 * @brief Returns whether fast-retransmit detection is currently enabled.
 *
 * @param tracker  The ACK tracker.
 * @return true if enabled, false if disabled or tracker is NULL.
 */
bool krs_ack_tracker_is_fast_retransmit_enabled(const AckTracker_t* tracker);

#endif // KRONOS_ACK_H
