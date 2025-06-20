#ifndef KRONOS_MATH_H
#define KRONOS_MATH_H
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Gets the value of a specific flag from a bitmask, treating bit 0 as the MSB (left-to-right).
 *
 * This function extracts a flag bit from a given data buffer where bits are indexed from the
 * most significant bit (bit 0) to the least significant bit (bit 7 * N - 1).
 *
 * @param flag_pos  Bit position of the flag (0 = MSB of first byte).
 * @param data      Pointer to the data buffer.
 * @param length    Length of the data buffer in bytes.
 * @return true if the bit is set, false otherwise.
 */
bool krs_math_bitmask_get_flag(uint8_t flag_pos, const uint8_t* data, uint8_t length);

#endif //KRONOS_MATH_H
