#ifndef KRONOS_CONGESTION_H
#define KRONOS_CONGESTION_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Opaque per-connection congestion controller using Slow Start + AIMD. */
typedef struct CongestionController CongestionController_t;

/**
 * @brief Creates a new congestion controller with default initial values.
 *
 * Initial state: cwnd=4, ssthresh=64, phase=SLOW_START, rto=1000ms.
 *
 * @return Pointer to the new CongestionController_t, or NULL on allocation failure.
 */
CongestionController_t* krs_congestion_create(void);

/**
 * @brief Destroys a congestion controller and frees its memory.
 *
 * @param cc  Pointer to the controller pointer; set to NULL on return.
 */
void krs_congestion_destroy(CongestionController_t** cc);

/**
 * @brief Checks whether the congestion window allows sending another packet.
 *
 * @param cc  The congestion controller.
 * @return true if in_flight < cwnd, false if window is full or cc is NULL.
 */
bool krs_congestion_can_send(const CongestionController_t* cc);

/**
 * @brief Records that a packet was sent (increments in_flight).
 *
 * Call this after successfully sending an ACK-tracked packet.
 *
 * @param cc  The congestion controller.
 */
void krs_congestion_on_send(CongestionController_t* cc);

/**
 * @brief Records that an ACK was received, adjusting the congestion window.
 *
 * During Slow Start: cwnd += 1 (exponential growth).
 * During Congestion Avoidance: cwnd += 1/cwnd (linear growth).
 * Also updates SRTT, RTTVAR, and RTO per RFC 6298.
 *
 * @param cc      The congestion controller.
 * @param rtt_ms  Round-trip time of the acknowledged packet in milliseconds.
 */
void krs_congestion_on_ack(CongestionController_t* cc, double rtt_ms);

/**
 * @brief Records a packet loss event (timeout or max retries exceeded).
 *
 * Sets ssthresh = cwnd/2, resets cwnd to KRS_CC_MIN_CWND,
 * and transitions to Slow Start phase.
 *
 * @param cc  The congestion controller.
 */
void krs_congestion_on_loss(CongestionController_t* cc);

/**
 * @brief Returns the current congestion window size in packets.
 *
 * @param cc  The congestion controller.
 * @return Current cwnd (floored to uint32_t), or 0 if cc is NULL.
 */
uint32_t krs_congestion_get_cwnd(const CongestionController_t* cc);

/**
 * @brief Returns the number of currently unacknowledged in-flight packets.
 *
 * @param cc  The congestion controller.
 * @return Current in_flight count, or 0 if cc is NULL.
 */
uint32_t krs_congestion_get_in_flight(const CongestionController_t* cc);

/**
 * @brief Returns the current computed retransmission timeout in milliseconds.
 *
 * @param cc  The congestion controller.
 * @return Current RTO in ms, or KRS_CC_INITIAL_RTO_MS if cc is NULL.
 */
double krs_congestion_get_rto(const CongestionController_t* cc);

/**
 * @brief Returns the current smoothed round-trip time estimate in milliseconds.
 *
 * @param cc  The congestion controller.
 * @return Current SRTT in ms, or 0.0 if no RTT samples received yet.
 */
double krs_congestion_get_srtt(const CongestionController_t* cc);

#endif // KRONOS_CONGESTION_H
