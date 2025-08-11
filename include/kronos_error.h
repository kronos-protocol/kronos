#ifndef KRONOS_ERROR_H
#define KRONOS_ERROR_H

#include <stdbool.h>

typedef struct OperationResult OperationResult_t;
typedef enum KronosError KronosError_e;

struct OperationResult {
    bool valid;
    int error_code;
};

enum KronosError {
    KRS_ERR_SUCCESS = 0,

    // General Errors(1-99)
    KRS_INVALID_ARGUMENT = 1,
    KRS_ERR_NULL_POINTER = 2,
    KRS_ERR_MEMORY_ALLOCATION = 3,
    KRS_ERR_BUFFER_TOO_SMALL = 4
};

#endif //KRONOS_ERROR_H
