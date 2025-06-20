#include "../include/kronos_math.h"

#include <winsock2.h>

bool krs_math_bitmask_get_flag(const uint8_t flag_pos, const uint8_t* data, const uint8_t length) {
    if (flag_pos  >= length * 8) {
        return false;
    }

    uint8_t big_endian_flag_pos = htonl(flag_pos);

    uint8_t array_pos = big_endian_flag_pos / 8;
    uint8_t bit_pos = big_endian_flag_pos % 8;

    return (data[array_pos] & (1 << (7 - bit_pos))) != 0; // Orders from left to right with Big Endian
}
