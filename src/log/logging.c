#import <kronos_log.h>
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

    char buffer[512]; // TODO: add this later to config with the char buffer pool from error messages
    const char* component_w_prefix = strcat("KRS_", component);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    DEFAULT_CALLBACK(level, component_w_prefix, buffer);
}