#ifndef KRONOS_ERROR_H
#define KRONOS_ERROR_H

#include <stdbool.h>

typedef struct OperationResult OperationResult_t;

struct OperationResult {
    bool valid;
    int error_code;
};

#endif //KRONOS_ERROR_H
