#include "kronos_log.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>


static KronosLogCallback_f s_callback = NULL;
static KronosLogLevel_e s_min_level = KRS_LOG_DEBUG;
static SRWLOCK s_log_lock = SRWLOCK_INIT;

void krs_log_callback_set(KronosLogCallback_f callback) {
    AcquireSRWLockExclusive(&s_log_lock);
    s_callback = callback;
    ReleaseSRWLockExclusive(&s_log_lock);
}

void krs_log_disable(void) {
    AcquireSRWLockExclusive(&s_log_lock);
    s_callback = NULL;
    ReleaseSRWLockExclusive(&s_log_lock);
}

void krs_log_set_level(KronosLogLevel_e level) {
    AcquireSRWLockExclusive(&s_log_lock);
    s_min_level = level;
    ReleaseSRWLockExclusive(&s_log_lock);
}

KronosLogLevel_e krs_log_get_level(void) {
    AcquireSRWLockShared(&s_log_lock);
    KronosLogLevel_e level = s_min_level;
    ReleaseSRWLockShared(&s_log_lock);
    return level;
}

void krs_log(KronosLogLevel_e level, const char* component, const char* format, ...) {
    AcquireSRWLockShared(&s_log_lock);
    KronosLogCallback_f cb = s_callback;
    KronosLogLevel_e min = s_min_level;
    ReleaseSRWLockShared(&s_log_lock);

    if (!cb) return;
    if (level < min) return;

    char buffer[512];
    char component_w_prefix[256];
    snprintf(component_w_prefix, sizeof(component_w_prefix), "KRS_%s", component);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    cb(level, component_w_prefix, buffer);
}
