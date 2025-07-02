#include <stdint.h>
#include "kronos_math.h"

uint16_t krs_math_uint16_from_uint8(const uint8_t ms_byte, const uint8_t ls_byte) {
    return ((uint16_t)ms_byte << 8) | ls_byte;
}

uint64_t krs_math_uint64_from_uint8(const uint8_t* source, const uint8_t offset, const uint16_t source_length) {
    if (offset + 8 > source_length) return 0; //TODO: proper error handling

    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= ((uint64_t)source[offset + i] << (8 * (7 - i))); // Big Endian Byte Order
    }
    return result;
}
