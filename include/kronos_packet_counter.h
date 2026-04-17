#ifndef KRONOS_PACKET_COUNTER_H
#define KRONOS_PACKET_COUNTER_H

#include "kronos_network.h"
#include <stdint.h>

/** @brief Opaque per-channel packet ID counter. */
typedef struct PacketCounter PacketCounter_t;

/**
 * @brief Creates a new packet counter with all channel counters initialized to zero.
 *
 * @return Pointer to the new PacketCounter_t, or NULL on allocation failure.
 */
PacketCounter_t* krs_packet_counter_create(void);

/**
 * @brief Destroys a packet counter and frees its memory.
 *
 * @param counter  Pointer to the counter pointer; set to NULL on return.
 */
void krs_packet_counter_destroy(PacketCounter_t** counter);

/**
 * @brief Increments and returns the next packet ID for the given channel.
 *
 * @param counter  The packet counter.
 * @param channel  Channel number (0–255).
 * @return The next packet ID for this channel, or 0 if counter is NULL.
 */
uint64_t krs_packet_counter_next(PacketCounter_t* counter, Channel_t channel);

/**
 * @brief Returns the current packet ID for the given channel without incrementing.
 *
 * @param counter  The packet counter.
 * @param channel  Channel number (0–255).
 * @return The current packet ID for this channel, or 0 if counter is NULL.
 */
uint64_t krs_packet_counter_current(const PacketCounter_t* counter, Channel_t channel);

/**
 * @brief Resets the packet ID counter for the given channel to zero.
 *
 * @param counter  The packet counter.
 * @param channel  Channel number (0–255).
 */
void krs_packet_counter_reset(PacketCounter_t* counter, Channel_t channel);

#endif // KRONOS_PACKET_COUNTER_H
