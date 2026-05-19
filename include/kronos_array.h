#ifndef KRONOS_ARRAY_H
#define KRONOS_ARRAY_H

#include "kronos_error.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief Opaque dynamic array holding void* pointers. */
typedef struct KrsArray KrsArray_t;

/** @brief Result type for array creation with explicit error handling. */
typedef struct KrsArrayCreateResult KrsArrayCreate_r;

struct KrsArrayCreateResult {
    KronosResult_b base;
    KrsArray_t* array;
};

/**
 * @brief Creates a new dynamic array with a given initial capacity.
 *
 * @param initial_capacity  Number of slots to pre-allocate. Uses a default if 0.
 * @return Pointer to the new KrsArray_t, or NULL on allocation failure.
 */
KrsArray_t* krs_array_create(uint32_t initial_capacity);

/**
 * @brief Creates a new dynamic array with explicit error handling.
 *
 * @param initial_capacity  Number of slots to pre-allocate. Uses a default if 0.
 * @return KrsArrayCreate_r containing the array or error information.
 *
 * @retval KRS_SUCCESS                Array created successfully.
 * @retval KRS_ERR_MEMORY_ALLOCATION  Allocation failed.
 */
KrsArrayCreate_r krs_array_create_s(uint32_t initial_capacity);

/**
 * @brief Destroys a dynamic array, freeing its internal storage.
 *
 * Items themselves are NOT freed. Use krs_array_destroy_items for that.
 *
 * @param array  Pointer to the array pointer; set to NULL on return.
 */
void krs_array_destroy(KrsArray_t** array);

/**
 * @brief Destroys a dynamic array, calling item_destroy on each non-NULL item first.
 *
 * @param array         Pointer to the array pointer; set to NULL on return.
 * @param item_destroy  Destructor called on each non-NULL item. May be NULL to skip.
 */
void krs_array_destroy_items(KrsArray_t** array, void (*item_destroy)(void*));

/**
 * @brief Appends an item pointer to the end of the array.
 *
 * Doubles capacity automatically if full.
 *
 * @param array  The array to push into.
 * @param item   The pointer to append.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                Item appended successfully.
 * @retval KRS_ERR_NULL_POINTER       array is NULL.
 * @retval KRS_ERR_MEMORY_ALLOCATION  Capacity growth failed.
 */
Void_r krs_array_push(KrsArray_t* array, void* item);

/**
 * @brief Returns the item pointer at the given index without removing it.
 *
 * @param array  The array to query.
 * @param index  Zero-based index.
 * @return The item pointer, or NULL if array is NULL or index is out of bounds.
 */
void* krs_array_get(const KrsArray_t* array, uint32_t index);

/**
 * @brief Removes and returns the last item pointer.
 *
 * @param array  The array to pop from.
 * @return The last item pointer, or NULL if array is NULL or empty.
 */
void* krs_array_pop(KrsArray_t* array);

/**
 * @brief Replaces the item pointer at the given index.
 *
 * @param array  The array to modify.
 * @param index  Zero-based index (must be within current length).
 * @param item   The new pointer to store.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS              Item replaced successfully.
 * @retval KRS_ERR_NULL_POINTER     array is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER index is out of bounds.
 */
Void_r krs_array_set(KrsArray_t* array, uint32_t index, void* item);

/**
 * @brief Removes the item at the given index, shifting subsequent items left.
 *
 * @param array  The array to modify.
 * @param index  Zero-based index of the item to remove.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS               Item removed successfully.
 * @retval KRS_ERR_NULL_POINTER      array is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER index is out of bounds.
 */
Void_r krs_array_remove(KrsArray_t* array, uint32_t index);

/**
 * @brief Returns the number of items currently in the array.
 *
 * @param array  The array to query.
 * @return Current item count, or 0 if array is NULL.
 */
uint32_t krs_array_length(const KrsArray_t* array);

/**
 * @brief Returns true if the array has no items.
 *
 * @param array  The array to query.
 * @return true if empty or NULL, false otherwise.
 */
bool krs_array_is_empty(const KrsArray_t* array);

/**
 * @brief Removes all items from the array without freeing them.
 *
 * Resets length to 0. Capacity is unchanged.
 *
 * @param array  The array to clear.
 */
void krs_array_clear(KrsArray_t* array);

/** @brief Type-safe get: returns a pointer of the given type at index. */
#define KRS_ARRAY_GET(array, index, type) ((type*)krs_array_get((array), (index)))

/** @brief Type-safe push: casts item_ptr to void* before appending. */
#define KRS_ARRAY_PUSH(array, item_ptr) krs_array_push((array), (void*)(item_ptr))

/** @brief Type-safe pop: casts the returned void* to the given type. */
#define KRS_ARRAY_POP(array, type) ((type*)krs_array_pop((array)))

#endif // KRONOS_ARRAY_H
