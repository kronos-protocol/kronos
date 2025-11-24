#include "malloc_wrapper.h"
#include <stdlib.h>
#include <string.h>

static enum {
    MOCK_MODE_NORMAL,
    MOCK_MODE_FAIL_ONCE,
    MOCK_MODE_FAIL_ALL,
    MOCK_MODE_CUSTOM
} mock_mode = MOCK_MODE_NORMAL;

static void* custom_return_ptr = NULL;
static int malloc_call_count = 0;
static int calloc_call_count = 0;
static int free_call_count = 0;
static size_t last_malloc_size = 0;
static size_t last_calloc_num = 0;
static size_t last_calloc_size = 0;

extern void* __real_malloc(size_t size);
extern void* __real_calloc(size_t num, size_t size);
extern void __real_free(void* ptr);
extern void* __real_realloc(void* ptr, size_t size);


void* __wrap_malloc(size_t size) {
    malloc_call_count++;
    last_malloc_size = size;

    switch (mock_mode) {
        case MOCK_MODE_FAIL_ONCE:
            mock_mode = MOCK_MODE_NORMAL;
            return NULL;

        case MOCK_MODE_FAIL_ALL:
            return NULL;

        case MOCK_MODE_CUSTOM:
            mock_mode = MOCK_MODE_NORMAL;
            return custom_return_ptr;

        default:
            return __real_malloc(size);
    }
}

void* __wrap_calloc(size_t num, size_t size) {
    calloc_call_count++;
    last_calloc_num = num;
    last_calloc_size = size;

    switch (mock_mode) {
        case MOCK_MODE_FAIL_ONCE:
            mock_mode = MOCK_MODE_NORMAL;
            return NULL;

        case MOCK_MODE_FAIL_ALL:
            return NULL;

        case MOCK_MODE_CUSTOM:
            mock_mode = MOCK_MODE_NORMAL;
            return custom_return_ptr;

        default:
            return __real_calloc(num, size);
    }
}

void __wrap_free(void* ptr) {
    free_call_count++;

    if (mock_mode == MOCK_MODE_CUSTOM && ptr == custom_return_ptr) {
        return;
    }

    __real_free(ptr);
}

void* __wrap_realloc(void* ptr, size_t size) {
    switch (mock_mode) {
        case MOCK_MODE_FAIL_ONCE:
            mock_mode = MOCK_MODE_NORMAL;
            return NULL;

        case MOCK_MODE_FAIL_ALL:
            return NULL;

        default:
            return __real_realloc(ptr, size);
    }
}

void mock_malloc_fail_next(void) {
    mock_mode = MOCK_MODE_FAIL_ONCE;
}

void mock_malloc_fail_all(void) {
    mock_mode = MOCK_MODE_FAIL_ALL;
}

void mock_malloc_succeed(void) {
    mock_mode = MOCK_MODE_NORMAL;
}

void mock_malloc_return_custom(void* ptr) {
    mock_mode = MOCK_MODE_CUSTOM;
    custom_return_ptr = ptr;
}

void mock_malloc_reset(void) {
    mock_mode = MOCK_MODE_NORMAL;
    custom_return_ptr = NULL;
    malloc_call_count = 0;
    calloc_call_count = 0;
    free_call_count = 0;
    last_malloc_size = 0;
    last_calloc_num = 0;
    last_calloc_size = 0;
}

int mock_malloc_get_call_count(void) {
    return malloc_call_count;
}

int mock_calloc_get_call_count(void) {
    return calloc_call_count;
}

int mock_free_get_call_count(void) {
    return free_call_count;
}

size_t mock_malloc_get_last_size(void) {
    return last_malloc_size;
}

void mock_malloc_get_last_calloc_params(size_t* num, size_t* size) {
    if (num) *num = last_calloc_num;
    if (size) *size = last_calloc_size;
}