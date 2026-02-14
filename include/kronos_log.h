#ifndef KRONOS_LOG_H
#define KRONOS_LOG_H

#include <stdarg.h>

typedef enum {
    KRS_LOG_DEBUG = 0,
    KRS_LOG_INFO  = 1,
    KRS_LOG_WARN  = 2,
    KRS_LOG_ERROR = 3,
    KRS_LOG_FATAL = 4
} KronosLogLevel_e;

typedef void (*KronosLogCallback_f)(
    KronosLogLevel_e level,
    const char* component,
    const char* message
);

void krs_log_callback_set(KronosLogCallback_f callback);
void krs_log_disable(void);
void krs_log(
    KronosLogLevel_e level,
    const char* component,
    const char* format,
    ...
);

// Override via -DKRS_LOG_LEVEL=KRS_LOG_DEBUG
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

#endif //KRONOS_LOG_H
