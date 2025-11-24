#ifndef KRONOS_ERROR_H
#define KRONOS_ERROR_H

#include <stdbool.h>

typedef struct VoidResult Void_r;
typedef struct KronosResultBase KronosResult_b;
typedef enum KronosError KronosError_e;

enum KronosError {
    KRS_ERR_SUCCESS = 0,

    // General Errors(1-99)
    KRS_INVALID_ARGUMENT = 1,
    KRS_ERR_NULL_POINTER = 2,
    KRS_ERR_MEMORY_ALLOCATION = 3,
    KRS_ERR_BUFFER_TOO_SMALL = 4
};

struct KronosResultBase {
    bool valid;
    KronosError_e error_code;
    const char* error_message;
    bool free_error_message;
};

struct VoidResult {
    KronosResult_b base;
};


static inline
KronosResult_b krs_lib_error_result_base_suc() {
    KronosResult_b result;
    result.valid = true;
    result.error_code = KRS_ERR_SUCCESS;
    result.error_message = NULL;
    result.free_error_message = false;
    return result;
}

static inline
KronosResult_b krs_lib_error_result_base_w_msg(const KronosError_e error_code, const char* error_message) {
    KronosResult_b result_base;
    result_base.valid = false;
    result_base.error_code = error_code;
    result_base.error_message = error_message;
    result_base.free_error_message = true;
    return result_base;
}

#endif //KRONOS_ERROR_H
