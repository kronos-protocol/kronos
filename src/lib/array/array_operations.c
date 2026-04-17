#include "kronos_array.h"
#include "array_internal.h"

#include <stdlib.h>


Void_r krs_array_push(KrsArray_t* array, void* item) {
    Void_r result = {0};
    if (!array) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "array is NULL");
        return result;
    }

    if (array->length >= array->capacity) {
        uint32_t new_capacity = array->capacity == 0 ? 8u : array->capacity * 2;
        void** new_items = realloc(array->items, sizeof(void*) * new_capacity);
        if (!new_items) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "Failed to grow array");
            return result;
        }
        array->items = new_items;
        array->capacity = new_capacity;
    }

    array->items[array->length++] = item;
    result.base = krs_lib_error_result_base_suc();
    return result;
}

void* krs_array_get(const KrsArray_t* array, uint32_t index) {
    if (!array || index >= array->length) return NULL;
    return array->items[index];
}

void* krs_array_pop(KrsArray_t* array) {
    if (!array || array->length == 0) return NULL;
    return array->items[--array->length];
}

Void_r krs_array_set(KrsArray_t* array, uint32_t index, void* item) {
    Void_r result = {0};
    if (!array) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "array is NULL");
        return result;
    }
    if (index >= array->length) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "index out of bounds");
        return result;
    }
    array->items[index] = item;
    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_array_remove(KrsArray_t* array, uint32_t index) {
    Void_r result = {0};
    if (!array) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "array is NULL");
        return result;
    }
    if (index >= array->length) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "index out of bounds");
        return result;
    }
    for (uint32_t i = index; i < array->length - 1; i++) {
        array->items[i] = array->items[i + 1];
    }
    array->length--;
    result.base = krs_lib_error_result_base_suc();
    return result;
}

uint32_t krs_array_length(const KrsArray_t* array) {
    if (!array) return 0;
    return array->length;
}

bool krs_array_is_empty(const KrsArray_t* array) {
    if (!array) return true;
    return array->length == 0;
}

void krs_array_clear(KrsArray_t* array) {
    if (!array) return;
    array->length = 0;
}
