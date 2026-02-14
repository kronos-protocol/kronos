#ifndef KRONOS_ERROR_H
#define KRONOS_ERROR_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct VoidResult Void_r;
typedef struct KronosResultBase KronosResult_b;
typedef enum KronosError KronosError_e;

enum KronosError {
    KRS_ERR_SUCCESS = 0,

    // General Errors (1–99)
    KRS_INVALID_ARGUMENT      = 1,
    KRS_ERR_NULL_POINTER      = 2,
    KRS_ERR_MEMORY_ALLOCATION = 3,
    KRS_ERR_BUFFER_TOO_SMALL  = 4
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
void krs_lib_error_result_base_cleanup(KronosResult_b krb)
{
    if (krb.free_error_message && krb.error_message) {
        free((void*)krb.error_message);
    }
}

static inline
KronosResult_b krs_lib_error_result_base_suc(void)
{
    return (KronosResult_b){
        .valid = true,
        .error_code = KRS_ERR_SUCCESS,
        .error_message = NULL,
        .free_error_message = false
    };
}

static inline
KronosResult_b
krs_lib_error_result_base_w_msg(
    KronosError_e error_code,
    const char* fmt,
    ...
) __attribute__((format(printf, 2, 3)));

static inline
KronosResult_b
krs_lib_error_result_base_w_msg(
    KronosError_e error_code,
    const char* fmt,
    ...
)
{
    (void)fmt;

    return (KronosResult_b){
        .valid = false,
        .error_code = error_code,
        .error_message = fmt,
        .free_error_message = false
    };
}

static inline
KronosResult_b
krs_lib_error_result_base_w_msgf(
    KronosError_e error_code,
    const char* fmt,
    ...
) __attribute__((format(printf, 2, 3)));

static inline
KronosResult_b
krs_lib_error_result_base_w_msgf(
    KronosError_e error_code,
    const char* fmt,
    ...
)
{
    KronosResult_b result = {
        .valid = false,
        .error_code = error_code,
        .error_message = NULL,
        .free_error_message = false
    };

    va_list ap;
    va_start(ap, fmt);

    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (len < 0) {
        va_end(ap);
        result.error_message = "error formatting message";
        return result;
    }

    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        va_end(ap);
        result.error_message = "out of memory";
        return result;
    }

    vsnprintf(buf, (size_t)len + 1, fmt, ap);
    va_end(ap);

    result.error_message = buf;
    result.free_error_message = true;
    return result;
}

#endif //KRONOS_ERROR_H
