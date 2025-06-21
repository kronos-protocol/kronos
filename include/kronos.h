#ifndef KRONOS_H
#define KRONOS_H
#include "../internal/kronos_internal.h"
#include <stdint.h>

typedef struct Frame Frame;

Frame krs_frame_create(const uint8_t* buffer, uint16_t received_bytes, uint8_t* stack_data_out, uint16_t stack_data_out_size);
Frame* krs_frame_create_heap(const uint8_t* buffer, uint16_t received_bytes);
void krs_frame_init(const uint8_t* buffer, uint16_t received_bytes, Frame* out, uint16_t out_data_size);

void krs_frame_destroy(Frame** frame);

#endif // KRONOS_H
