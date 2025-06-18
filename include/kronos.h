#ifndef KRONOS_H
#define KRONOS_H

typedef struct Frame Frame;

Frame krs_frame_create(char* buffer);
Frame* krs_frame_create_heap(char* buffer);
void krs_frame_init(char* buffer, Frame* out);


#endif //KRONOS_H
