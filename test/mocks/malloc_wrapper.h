#ifndef MALLOC_WRAPPER_H
#define MALLOC_WRAPPER_H

#include <stddef.h>

/**
 * Make the next malloc/calloc call fail (return NULL)
 * Automatically resets to normal after one call
 */
void mock_malloc_fail_next(void);

/**
 * Make all malloc/calloc calls fail until mock_malloc_reset() is called
 */
void mock_malloc_fail_all(void);

/**
 * Resume normal malloc/calloc behavior (pass through to real functions)
 */
void mock_malloc_succeed(void);

/**
 * Make the next malloc/calloc return a specific pointer
 * Useful for returning stack-allocated buffers in tests
 * @param ptr The pointer to return (caller manages lifetime)
 */
void mock_malloc_return_custom(void* ptr);

/**
 * Reset all mock state to defaults
 * Should be called in setUp() or tearDown()
 */
void mock_malloc_reset(void);

// region Query Commands

/**
 * Get the number of times malloc was called since last reset
 */
int mock_malloc_get_call_count(void);

/**
 * Get the number of times calloc was called since last reset
 */
int mock_calloc_get_call_count(void);

/**
 * Get the number of times free was called since last reset
 */
int mock_free_get_call_count(void);

/**
 * Get the size parameter from the last malloc call
 */
size_t mock_malloc_get_last_size(void);

/**
 * Get the parameters from the last calloc call
 * @param num Output parameter for number of elements (can be NULL)
 * @param size Output parameter for size of each element (can be NULL)
 */
void mock_malloc_get_last_calloc_params(size_t* num, size_t* size);

// endregion

#endif // MALLOC_WRAPPER_H
