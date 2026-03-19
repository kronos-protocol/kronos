#include "kronos_log.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static KronosLogCallback_f DEFAULT_CALLBACK = NULL;

void krs_log_callback_set(KronosLogCallback_f callback) {
    DEFAULT_CALLBACK = callback;
}

void krs_log_disable(void) {
    DEFAULT_CALLBACK = NULL;
}

void krs_log(KronosLogLevel_e level, const char* component, const char* format, ...) {
    if (!DEFAULT_CALLBACK) return;

    char buffer[512];
    char component_w_prefix[256];
    snprintf(component_w_prefix, sizeof(component_w_prefix), "KRS_%s", component);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    DEFAULT_CALLBACK(level, component_w_prefix, buffer);
}