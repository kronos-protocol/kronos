#include "kronos.h"


uint16_t _krs_frame_calculate_data_length(const uint16_t received_bytes) {
    return received_bytes - KRONOS_FRAME_HEADER_LENGTH;
}
