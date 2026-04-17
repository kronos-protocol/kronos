#include "kronos_array.h"
#include "array_internal.h"

#include <stdlib.h>

#define DEFAULT_CAPACITY 8u


KrsArray_t* krs_array_create(uint32_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = DEFAULT_CAPACITY;

    KrsArray_t* array = malloc(sizeof(KrsArray_t));
    if (!array) return NULL;

    array->items = malloc(sizeof(void*) * initial_capacity);
    if (!array->items) {
        free(array);
        return NULL;
    }

    array->length = 0;
    array->capacity = initial_capacity;
    return array;
}

KrsArrayCreate_r krs_array_create_s(uint32_t initial_capacity) {
    KrsArrayCreate_r result = {0};
    KrsArray_t* array = krs_array_create(initial_capacity);
    if (!array) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "Failed to allocate array");
        return result;
    }
    result.base = krs_lib_error_result_base_suc();
    result.array = array;
    return result;
}

void krs_array_destroy(KrsArray_t** array) {
    if (!array || !*array) return;
    free((*array)->items);
    free(*array);
    *array = NULL;
}

void krs_array_destroy_items(KrsArray_t** array, void (*item_destroy)(void*)) {
    if (!array || !*array) return;
    if (item_destroy) {
        for (uint32_t i = 0; i < (*array)->length; i++) {
            if ((*array)->items[i]) {
                item_destroy((*array)->items[i]);
            }
        }
    }
    krs_array_destroy(array);
}
