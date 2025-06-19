#ifndef KRONOS_H
#define KRONOS_H
#include "../internal/kronos_internal.h"
#include <stdint.h>

typedef struct Frame Frame;

Frame krs_frame_create(const uint8_t* buffer, uint8_t* stack_data_out, uint16_t received_bytes);
Frame* krs_frame_create_heap(uint8_t* buffer, uint16_t received_bytes);
void krs_frame_init(uint8_t* buffer, uint16_t received_bytes, Frame* out);

#endif // KRONOS_H
