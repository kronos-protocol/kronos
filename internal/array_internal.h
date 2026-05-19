#ifndef ARRAY_INTERNAL_H
#define ARRAY_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>

struct KrsArray {
    void** items;
    uint32_t length;
    uint32_t capacity;
};

#endif //ARRAY_INTERNAL_H
