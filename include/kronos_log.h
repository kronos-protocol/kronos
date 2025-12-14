#ifndef KRONOS_LOG_H
#define KRONOS_LOG_H

typedef enum KronosLogLevel KronosLogLevel_e;

typedef void (*KronosLogCallback_f) (KronosLogLevel_e log_level, const char* component, const char* message);


enum KronosLogLevel {
    KRS_LOG_DEBUG = 0,
    KRS_LOG_INFO = 1,
    KRS_LOG_WARN = 2,
    KRS_LOG_ERROR = 3,
    KRS_LOG_FATAL = 4,
};


void krs_log_callback_set(KronosLogCallback_f callback);
void krs_log_disable(void);
void krs_log(KronosLogLevel_e level, const char* component, const char* format, ...);


#define KRS_LOG_DEBUG(component, fmt, ...) \
krs_log(KRS_LOG_DEBUG, component, fmt, ##__VA_ARGS__)
#define KRS_LOG_INFO(component, fmt, ...) \
krs_log(KRS_LOG_INFO, component, fmt, ##__VA_ARGS__)
#define KRS_LOG_WARN(component, fmt, ...) \
krs_log(KRS_LOG_WARN, component, fmt, ##__VA_ARGS__)
#define KRS_LOG_ERROR(component, fmt, ...) \
krs_log(KRS_LOG_ERROR, component, fmt, ##__VA_ARGS__)

#endif //KRONOS_LOG_H
