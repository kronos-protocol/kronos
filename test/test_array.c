#include "malloc_wrapper.h"
#include "unity.h"
#include "array_internal.h"
#include "kronos_array.h"


static int destroy_call_count = 0;

static void item_destroy_counter(void* item) {
    (void)item;
    destroy_call_count++;
}


void test_array_create_destroy(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL_MESSAGE(arr, "Array creation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(arr->items, "Array items buffer is NULL");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, arr->length, "Initial length should be 0");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4, arr->capacity, "Initial capacity should match argument");

    krs_array_destroy(&arr);
    TEST_ASSERT_NULL_MESSAGE(arr, "Array pointer should be NULL after destroy");
}

void test_array_create_default_capacity(void) {
    KrsArray_t* arr = krs_array_create(0);
    TEST_ASSERT_NOT_NULL_MESSAGE(arr, "Array creation with 0 capacity failed");
    TEST_ASSERT_TRUE_MESSAGE(arr->capacity > 0, "Default capacity should be greater than 0");
    krs_array_destroy(&arr);
}

void test_array_create_s_success(void) {
    KrsArrayCreate_r result = krs_array_create_s(8);
    TEST_ASSERT_TRUE_MESSAGE(result.base.valid, "Result should be valid");
    TEST_ASSERT_EQUAL_INT_MESSAGE(KRS_SUCCESS, result.base.error_code, "Error code should be KRS_SUCCESS");
    TEST_ASSERT_NOT_NULL_MESSAGE(result.array, "Array pointer should not be NULL");
    krs_array_destroy(&result.array);
}

void test_array_create_s_malloc_failure(void) {
    mock_malloc_fail_next();
    KrsArrayCreate_r result = krs_array_create_s(8);
    TEST_ASSERT_FALSE_MESSAGE(result.base.valid, "Result should be invalid on malloc failure");
    TEST_ASSERT_EQUAL_INT_MESSAGE(KRS_ERR_MEMORY_ALLOCATION, result.base.error_code,
        "Error code should be KRS_ERR_MEMORY_ALLOCATION");
    TEST_ASSERT_NULL_MESSAGE(result.array, "Array pointer should be NULL on failure");
}

void test_array_push_get(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int a = 1, b = 2, c = 3;
    Void_r r1 = krs_array_push(arr, &a);
    Void_r r2 = krs_array_push(arr, &b);
    Void_r r3 = krs_array_push(arr, &c);

    TEST_ASSERT_TRUE_MESSAGE(r1.base.valid, "Push 1 should succeed");
    TEST_ASSERT_TRUE_MESSAGE(r2.base.valid, "Push 2 should succeed");
    TEST_ASSERT_TRUE_MESSAGE(r3.base.valid, "Push 3 should succeed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3, krs_array_length(arr), "Length should be 3");

    TEST_ASSERT_EQUAL_PTR_MESSAGE(&a, krs_array_get(arr, 0), "Item 0 should be &a");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&b, krs_array_get(arr, 1), "Item 1 should be &b");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&c, krs_array_get(arr, 2), "Item 2 should be &c");

    krs_array_destroy(&arr);
}

void test_array_get_out_of_bounds(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int x = 42;
    krs_array_push(arr, &x);

    TEST_ASSERT_NULL_MESSAGE(krs_array_get(arr, 1), "Out-of-bounds get should return NULL");
    TEST_ASSERT_NULL_MESSAGE(krs_array_get(arr, 100), "Large index get should return NULL");
    TEST_ASSERT_NULL_MESSAGE(krs_array_get(NULL, 0), "NULL array get should return NULL");

    krs_array_destroy(&arr);
}

void test_array_pop(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int a = 10, b = 20;
    krs_array_push(arr, &a);
    krs_array_push(arr, &b);

    void* popped = krs_array_pop(arr);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&b, popped, "Pop should return last item");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, krs_array_length(arr), "Length should be 1 after pop");

    popped = krs_array_pop(arr);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&a, popped, "Second pop should return first item");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, krs_array_length(arr), "Length should be 0 after second pop");

    popped = krs_array_pop(arr);
    TEST_ASSERT_NULL_MESSAGE(popped, "Pop on empty array should return NULL");

    krs_array_destroy(&arr);
}

void test_array_auto_grow(void) {
    KrsArray_t* arr = krs_array_create(2);
    TEST_ASSERT_NOT_NULL(arr);

    int items[10];
    for (int i = 0; i < 10; i++) {
        items[i] = i;
        Void_r r = krs_array_push(arr, &items[i]);
        TEST_ASSERT_TRUE_MESSAGE(r.base.valid, "Push should succeed during auto-grow");
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(10, krs_array_length(arr), "Length should be 10 after 10 pushes");
    TEST_ASSERT_TRUE_MESSAGE(arr->capacity >= 10, "Capacity should have grown to at least 10");

    for (int i = 0; i < 10; i++) {
        int* val = KRS_ARRAY_GET(arr, (uint32_t)i, int);
        TEST_ASSERT_NOT_NULL(val);
        TEST_ASSERT_EQUAL_INT_MESSAGE(i, *val, "Item value should match");
    }

    krs_array_destroy(&arr);
}

void test_array_remove_shifts_left(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int a = 1, b = 2, c = 3;
    krs_array_push(arr, &a);
    krs_array_push(arr, &b);
    krs_array_push(arr, &c);

    Void_r r = krs_array_remove(arr, 1);
    TEST_ASSERT_TRUE_MESSAGE(r.base.valid, "Remove should succeed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, krs_array_length(arr), "Length should be 2 after remove");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&a, krs_array_get(arr, 0), "Item 0 should still be &a");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&c, krs_array_get(arr, 1), "Item 1 should be &c after shift");

    krs_array_destroy(&arr);
}

void test_array_remove_out_of_bounds(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int x = 1;
    krs_array_push(arr, &x);

    Void_r r = krs_array_remove(arr, 5);
    TEST_ASSERT_FALSE_MESSAGE(r.base.valid, "Out-of-bounds remove should fail");
    TEST_ASSERT_EQUAL_INT_MESSAGE(KRS_ERR_INVALID_PARAMETER, r.base.error_code,
        "Error should be KRS_ERR_INVALID_PARAMETER");

    krs_array_destroy(&arr);
}

void test_array_set(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int a = 1, b = 99;
    krs_array_push(arr, &a);

    Void_r r = krs_array_set(arr, 0, &b);
    TEST_ASSERT_TRUE_MESSAGE(r.base.valid, "Set should succeed");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&b, krs_array_get(arr, 0), "Item 0 should be &b after set");

    Void_r r2 = krs_array_set(arr, 5, &b);
    TEST_ASSERT_FALSE_MESSAGE(r2.base.valid, "Out-of-bounds set should fail");

    krs_array_destroy(&arr);
}

void test_array_is_empty(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    TEST_ASSERT_TRUE_MESSAGE(krs_array_is_empty(arr), "New array should be empty");
    TEST_ASSERT_TRUE_MESSAGE(krs_array_is_empty(NULL), "NULL array should report empty");

    int x = 1;
    krs_array_push(arr, &x);
    TEST_ASSERT_FALSE_MESSAGE(krs_array_is_empty(arr), "Array with item should not be empty");

    krs_array_destroy(&arr);
}

void test_array_clear(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int a = 1, b = 2;
    krs_array_push(arr, &a);
    krs_array_push(arr, &b);

    krs_array_clear(arr);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, krs_array_length(arr), "Length should be 0 after clear");
    TEST_ASSERT_TRUE_MESSAGE(arr->capacity >= 2, "Capacity should be unchanged after clear");

    krs_array_destroy(&arr);
}

void test_array_destroy_items(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int* a = malloc(sizeof(int));
    int* b = malloc(sizeof(int));
    *a = 10;
    *b = 20;

    krs_array_push(arr, a);
    krs_array_push(arr, b);

    destroy_call_count = 0;
    krs_array_destroy_items(&arr, item_destroy_counter);

    TEST_ASSERT_NULL_MESSAGE(arr, "Array pointer should be NULL after destroy_items");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, destroy_call_count, "Destructor should be called for each item");
}

void test_array_push_null_array(void) {
    Void_r r = krs_array_push(NULL, (void*)1);
    TEST_ASSERT_FALSE_MESSAGE(r.base.valid, "Push to NULL array should fail");
    TEST_ASSERT_EQUAL_INT_MESSAGE(KRS_ERR_NULL_POINTER, r.base.error_code,
        "Error should be KRS_ERR_NULL_POINTER");
}

void test_array_macros(void) {
    KrsArray_t* arr = krs_array_create(4);
    TEST_ASSERT_NOT_NULL(arr);

    int a = 42;
    KRS_ARRAY_PUSH(arr, &a);

    int* val = KRS_ARRAY_GET(arr, 0, int);
    TEST_ASSERT_NOT_NULL(val);
    TEST_ASSERT_EQUAL_INT_MESSAGE(42, *val, "KRS_ARRAY_GET should return correct value");

    int* popped = KRS_ARRAY_POP(arr, int);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&a, popped, "KRS_ARRAY_POP should return last item");

    krs_array_destroy(&arr);
}
