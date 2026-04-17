#ifndef KRONOS_ACK_H
#define KRONOS_ACK_H

#include <stdint.h>

/** @brief Opaque selective ACK tracker. */
typedef struct AckTracker AckTracker_t;

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
 * @param frame_data  Raw frame bytes to retain for potential retransmission.
 * @param frame_size  Number of bytes in frame_data.
 */
void krs_ack_tracker_expect(AckTracker_t* tracker, uint64_t packet_id,
                            const uint8_t* frame_data, uint16_t frame_size);

/**
 * @brief Records that a packet was acknowledged, removing it from tracking.
 *
 * No-op if tracker is NULL or packet_id is not found.
 *
 * @param tracker          The tracker to update.
 * @param acked_packet_id  ID of the packet that was acknowledged.
 */
void krs_ack_tracker_receive(AckTracker_t* tracker, uint64_t acked_packet_id);

/**
 * @brief Records that a packet was acknowledged and returns the RTT sample.
 *
 * Removes all entries matching acked_packet_id and returns the RTT
 * (in milliseconds) of the first matching entry. Returns -1.0 if
 * the tracker is NULL or the packet_id is not found.
 *
 * @param tracker          The tracker to update.
 * @param acked_packet_id  ID of the packet that was acknowledged.
 * @return RTT in milliseconds, or -1.0 if not found.
 */
double krs_ack_tracker_receive_rtt(AckTracker_t* tracker, uint64_t acked_packet_id);

/**
 * @brief Checks for timed-out packets and returns their IDs for retransmission.
 *
 * Packets that have exceeded max_retries are silently dropped. For each
 * remaining timed-out packet, its ID is written to retry_ids_out (up to
 * out_capacity entries), its retry count is incremented, and its timeout
 * timer is reset.
 *
 * @param tracker        The tracker to check.
 * @param retry_ids_out  Output buffer for packet IDs that need retransmission.
 * @param out_capacity   Maximum number of IDs that fit in retry_ids_out.
 * @return Total number of packets that timed out (may exceed out_capacity).
 */
uint32_t krs_ack_tracker_check_timeouts(AckTracker_t* tracker,
                                        uint64_t* retry_ids_out, uint32_t out_capacity);

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
 * @brief Updates the timeout used for detecting timed-out packets.
 *
 * Allows the congestion controller to feed its computed RTO back
 * to the tracker for adaptive retransmission timing.
 *
 * @param tracker     The ACK tracker.
 * @param timeout_ms  New timeout in milliseconds. Clamped to minimum 50ms.
 */
void krs_ack_tracker_set_timeout(AckTracker_t* tracker, uint32_t timeout_ms);

#endif // KRONOS_ACK_H
