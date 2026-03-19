#ifndef KRONOS_LOG_H
#define KRONOS_LOG_H

#include <stdarg.h>

/**
 * @brief Log severity levels, ordered from least to most severe.
 */
typedef enum {
    KRS_LOG_DEBUG = 0,
    KRS_LOG_INFO  = 1,
    KRS_LOG_WARN  = 2,
    KRS_LOG_ERROR = 3,
    KRS_LOG_FATAL = 4
} KronosLogLevel_e;

/**
 * @brief Function pointer type for log output callbacks.
 *
 * @param level      Severity of the log message.
 * @param component  Null-terminated component/module name (prefixed with "KRS_").
 * @param message    Null-terminated formatted log message.
 */
typedef void (*KronosLogCallback_f)(KronosLogLevel_e level, const char* component, const char* message);

/**
 * @brief Registers a callback to receive all log output.
 *
 * Replaces any previously registered callback.
 *
 * @param callback  The function to call for each log message.
 */
void krs_log_callback_set(KronosLogCallback_f callback);

/**
 * @brief Disables all log output by clearing the registered callback.
 */
void krs_log_disable(void);

/**
 * @brief Emits a log message at the given severity level.
 *
 * No-op if no callback is registered. The component name is prefixed with "KRS_"
 * before being forwarded to the callback.
 *
 * @param level      Severity level.
 * @param component  Module name (without prefix).
 * @param format     printf-style format string.
 * @param ...        Format arguments.
 */
void krs_log(KronosLogLevel_e level, const char* component, const char* format, ...);

#ifndef KRS_LOG_LEVEL
#define KRS_LOG_LEVEL KRS_LOG_DEBUG
#endif

#define KRS_LOG_FMT(fmt) "%s: " fmt

#if KRS_LOG_LEVEL <= KRS_LOG_DEBUG
#define KRS_LOG_DEBUG(component, fmt, ...) krs_log(KRS_LOG_DEBUG, component, KRS_LOG_FMT(fmt), __func__, ##__VA_ARGS__)
#else
#define KRS_LOG_DEBUG(component, fmt, ...) ((void)0)
#endif

#if KRS_LOG_LEVEL <= KRS_LOG_INFO
#define KRS_LOG_INFO(component, fmt, ...) krs_log(KRS_LOG_INFO, component, KRS_LOG_FMT(fmt), __func__, ##__VA_ARGS__)
#else
#define KRS_LOG_INFO(component, fmt, ...) ((void)0)
#endif

#if KRS_LOG_LEVEL <= KRS_LOG_WARN
#define KRS_LOG_WARN(component, fmt, ...) krs_log(KRS_LOG_WARN, component, KRS_LOG_FMT(fmt), __func__, ##__VA_ARGS__)
#else
#define KRS_LOG_WARN(component, fmt, ...) ((void)0)
#endif

#if KRS_LOG_LEVEL <= KRS_LOG_ERROR
#define KRS_LOG_ERROR(component, fmt, ...) krs_log(KRS_LOG_ERROR, component, KRS_LOG_FMT(fmt), __func__, ##__VA_ARGS__)
#else
#define KRS_LOG_ERROR(component, fmt, ...) ((void)0)
#endif

#if KRS_LOG_LEVEL <= KRS_LOG_FATAL
#define KRS_LOG_FATAL(component, fmt, ...) krs_log(KRS_LOG_FATAL, component, KRS_LOG_FMT(fmt), __func__, ##__VA_ARGS__)
#else
#define KRS_LOG_FATAL(component, fmt, ...) ((void)0)
#endif

#endif // KRONOS_LOG_H
