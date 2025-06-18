#ifndef KRONOS_H
#define KRONOS_H
#include <stdint.h>

typedef struct Frame Frame;

Frame krs_frame_create(uint8_t* buffer, uint16_t received_bytes);
Frame* krs_frame_create_heap(uint8_t* buffer, uint16_t received_bytes);
void krs_frame_init(uint8_t* buffer, uint16_t received_bytes, Frame* out);


#endif //KRONOS_H
