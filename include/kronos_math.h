#ifndef KRONOS_MATH_H
#define KRONOS_MATH_H
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Returns the value of a flag bit at a given position in a big-endian bitmask.
 *
 * Bits are numbered left-to-right: position 0 is the MSB of the first byte.
 *
 * @param flag_pos  Bit position (0 = MSB of data[0]).
 * @param data      Pointer to the bitmask buffer.
 * @param length    Length of the data buffer in bytes.
 * @return true if the bit is set, false otherwise or if flag_pos is out of range.
 */
bool krs_math_bitmask_get_flag(uint8_t flag_pos, const uint8_t* data, uint8_t length);

/**
 * @brief Sets a flag bit at a given position in a big-endian bitmask.
 *
 * Bits are numbered left-to-right: position 0 is the MSB of the first byte.
 *
 * @param flag_pos  Bit position (0 = MSB of data[0]).
 * @param data      Pointer to the bitmask buffer to modify.
 * @param length    Length of the data buffer in bytes.
 */
void krs_math_bitmask_set_flag(uint8_t flag_pos, uint8_t* data, uint8_t length);

/**
 * @brief Combines two bytes into a big-endian uint16_t.
 *
 * @param ms_byte  Most significant byte.
 * @param ls_byte  Least significant byte.
 * @return Combined 16-bit value: (ms_byte << 8) | ls_byte.
 */
uint16_t krs_math_uint16_from_uint8(uint8_t ms_byte, uint8_t ls_byte);

/**
 * @brief Reads 8 consecutive bytes from a buffer at a given offset and returns a big-endian uint64_t.
 *
 * @param source         Source buffer.
 * @param offset         Byte offset within source to start reading from.
 * @param source_length  Total length of the source buffer (used for bounds checking).
 * @return Combined 64-bit value, or 0 if the read would exceed source_length.
 */
uint64_t krs_math_uint64_from_uint8(const uint8_t* source, uint8_t offset, uint16_t source_length);

#endif // KRONOS_MATH_H
