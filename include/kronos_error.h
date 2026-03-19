#ifndef KRONOS_ERROR_H
#define KRONOS_ERROR_H

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct VoidResult Void_r;
typedef struct KronosResultBase KronosResult_b;
typedef enum KronosError KronosError_e;

/** @brief Alias for KronosError_e — preferred in function signatures. */
typedef KronosError_e krs_error_t;

/**
 * @brief Error codes for the Kronos protocol library.
 *
 * Organized in HTTP-style numeric ranges by category.
 * Use KRS_SUCCESS (0) to test success.
 */
enum KronosError {
    KRS_SUCCESS = 0,

    KRS_ERR_NULL_POINTER        = 1,
    KRS_ERR_INVALID_PARAMETER   = 2,
    KRS_ERR_MEMORY_ALLOCATION   = 3,
    KRS_ERR_BUFFER_TOO_SMALL    = 4,
    KRS_ERR_NOT_INITIALIZED     = 5,
    KRS_ERR_ALREADY_INITIALIZED = 6,
    KRS_ERR_NOT_NULL_TERMINATED = 7,
    KRS_ERR_TOO_LONG            = 8,

    KRS_ERR_FRAME_INVALID_HEADER      = 100,
    KRS_ERR_FRAME_INVALID_PROTOCOL    = 101,
    KRS_ERR_FRAME_CORRUPT_DATA        = 102,
    KRS_ERR_FRAME_UNSUPPORTED_VERSION = 103,
    KRS_ERR_FRAME_INVALID_TYPE        = 104,
    KRS_ERR_FRAME_BODY_TOO_LARGE      = 105,
    KRS_ERR_FRAME_ALREADY_FREED       = 106,

    KRS_ERR_NETWORK_INVALID_IP        = 200,
    KRS_ERR_NETWORK_INVALID_PORT      = 201,
    KRS_ERR_NETWORK_CONNECTION_FAILED = 202,
    KRS_ERR_NETWORK_SOCKET_ERROR      = 203,
    KRS_ERR_NETWORK_TIMEOUT           = 204,
    KRS_ERR_NETWORK_ADDRESS_IN_USE    = 205,
    KRS_ERR_NETWORK_UNREACHABLE       = 206,

    KRS_ERR_SERVER_PORT_IN_USE      = 300,
    KRS_ERR_SERVER_MAX_CONNECTIONS  = 301,
    KRS_ERR_SERVER_NOT_LISTENING    = 302,
    KRS_ERR_SERVER_BIND_FAILED      = 303,
    KRS_ERR_SERVER_ALREADY_RUNNING  = 304,

    KRS_ERR_CLIENT_NOT_CONNECTED    = 400,
    KRS_ERR_CLIENT_CONNECTION_LOST  = 401,
    KRS_ERR_CLIENT_HANDSHAKE_FAILED = 402,
    KRS_ERR_CLIENT_AUTH_FAILED      = 403,
    KRS_ERR_CLIENT_TIMEOUT          = 404,

    KRS_ERR_MATH_OVERFLOW         = 500,
    KRS_ERR_MATH_UNDERFLOW        = 501,
    KRS_ERR_MATH_INVALID_BITMASK  = 502,
    KRS_ERR_MATH_DIVISION_BY_ZERO = 503,

    KRS_ERR_PLATFORM_WINDOWS_SOCKET = 600,
    KRS_ERR_PLATFORM_LINUX_EPOLL    = 650,
    KRS_ERR_PLATFORM_UNSUPPORTED    = 699
};

/**
 * @brief Error category groupings corresponding to the numeric ranges in KronosError_e.
 */
typedef enum {
    KRS_ERR_CATEGORY_SUCCESS,
    KRS_ERR_CATEGORY_GENERAL,
    KRS_ERR_CATEGORY_FRAME,
    KRS_ERR_CATEGORY_NETWORK,
    KRS_ERR_CATEGORY_SERVER,
    KRS_ERR_CATEGORY_CLIENT,
    KRS_ERR_CATEGORY_MATH,
    KRS_ERR_CATEGORY_PLATFORM
} KronosErrorCategory_e;

/**
 * @brief Base result struct embedded in all result types.
 *
 * @note Call krs_lib_error_result_base_cleanup() when free_error_message is true.
 */
struct KronosResultBase {
    bool valid;
    KronosError_e error_code;
    const char* error_message;
    bool free_error_message;
};

/**
 * @brief Result type for void operations that may fail.
 */
struct VoidResult {
    KronosResult_b base;
};

/**
 * @brief Returns a human-readable string describing the error code.
 *
 * @param error  The error code to describe.
 * @return Pointer to a static string describing the error. Never NULL.
 */
const char* krs_error_get_message(krs_error_t error);

/**
 * @brief Returns the category of a given error code.
 *
 * @param error  The error code to categorize.
 * @return The krs_error_category_t indicating which numeric range the error belongs to.
 */
KronosErrorCategory_e krs_error_get_category(krs_error_t error);

/**
 * @brief Returns whether the given error indicates an unrecoverable condition.
 *
 * @param error  The error code to test.
 * @return true if the error is fatal, false otherwise.
 */
bool krs_error_is_fatal(krs_error_t error);

//TODO: move temporary inline to .c file
/**
 * @brief Frees any dynamically allocated error message stored in a result base.
 *
 * @param krb  The result base whose message should be freed if needed.
 */
static inline void krs_lib_error_result_base_cleanup(KronosResult_b krb) {
    if (krb.free_error_message && krb.error_message) {
        free((void*)krb.error_message);
    }
}

/**
 * @brief Creates a successful (valid=true, KRS_SUCCESS) result base.
 *
 * @return KronosResult_b with valid=true and error_code=KRS_SUCCESS.
 */
static inline KronosResult_b krs_lib_error_result_base_suc(void) {
    return (KronosResult_b){
        .valid = true,
        .error_code = KRS_SUCCESS,
        .error_message = NULL,
        .free_error_message = false
    };
}

static inline KronosResult_b krs_lib_error_result_base_w_msg(KronosError_e error_code, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief Creates a failed result base using the format string as a static error message.
 *
 * @param error_code  The error code.
 * @param fmt         Message string stored as the error message (not formatted, no allocation).
 * @return KronosResult_b with valid=false.
 */
static inline KronosResult_b krs_lib_error_result_base_w_msg(KronosError_e error_code, const char* fmt, ...) {
    (void)fmt;
    return (KronosResult_b){
        .valid = false,
        .error_code = error_code,
        .error_message = fmt,
        .free_error_message = false
    };
}

static inline KronosResult_b krs_lib_error_result_base_w_msgf(KronosError_e error_code, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief Creates a failed result base with a heap-allocated formatted error message.
 *
 * @param error_code  The error code.
 * @param fmt         printf-style format string for the error message.
 * @return KronosResult_b with valid=false. Caller must call krs_lib_error_result_base_cleanup().
 */
static inline KronosResult_b krs_lib_error_result_base_w_msgf(KronosError_e error_code, const char* fmt, ...) {
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

#endif // KRONOS_ERROR_H
