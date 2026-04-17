#include "kronos_server.h"
#include "message_queue_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <stdlib.h>
#include <string.h>


static IncomingMessage_t* s_make_msg(uint8_t tag, Port_t port) {
    IncomingMessage_t* msg = malloc(sizeof(IncomingMessage_t));
    if (!msg) return NULL;
    msg->data[0] = tag;
    msg->data_length = 1;
    msg->port = port;
    memset(&msg->remote_address, 0, sizeof(msg->remote_address));
    return msg;
}

void test_mq_create_destroy(void) {
    MessageQueue_t* q = krs_message_queue_create(16);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(16, q->capacity);
    TEST_ASSERT_EQUAL_size_t(0, q->count);

    krs_message_queue_destroy(&q);
    TEST_ASSERT_NULL(q);
}

void test_mq_create_zero_capacity(void) {
    MessageQueue_t* q = krs_message_queue_create(0);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(64, q->capacity);

    krs_message_queue_destroy(&q);
}

void test_mq_push_pop_single(void) {
    MessageQueue_t* q = krs_message_queue_create(16);
    TEST_ASSERT_NOT_NULL(q);

    IncomingMessage_t* msg = s_make_msg(0xAA, 8080);
    TEST_ASSERT_NOT_NULL(msg);

    krs_message_queue_push(q, msg);
    TEST_ASSERT_EQUAL_size_t(1, q->count);

    IncomingMessage_t* popped = krs_message_queue_pop(q, 100);
    TEST_ASSERT_NOT_NULL(popped);
    TEST_ASSERT_EQUAL_size_t(0, q->count);
    TEST_ASSERT_EQUAL_UINT8(0xAA, popped->data[0]);
    TEST_ASSERT_EQUAL_UINT16(8080, popped->port);

    krs_message_queue_msg_destroy(&popped);
    krs_message_queue_destroy(&q);
}

void test_mq_push_pop_fifo_order(void) {
    MessageQueue_t* q = krs_message_queue_create(16);

    krs_message_queue_push(q, s_make_msg(1, 100));
    krs_message_queue_push(q, s_make_msg(2, 100));
    krs_message_queue_push(q, s_make_msg(3, 100));

    IncomingMessage_t* m1 = krs_message_queue_pop(q, 100);
    IncomingMessage_t* m2 = krs_message_queue_pop(q, 100);
    IncomingMessage_t* m3 = krs_message_queue_pop(q, 100);

    TEST_ASSERT_EQUAL_UINT8(1, m1->data[0]);
    TEST_ASSERT_EQUAL_UINT8(2, m2->data[0]);
    TEST_ASSERT_EQUAL_UINT8(3, m3->data[0]);

    krs_message_queue_msg_destroy(&m1);
    krs_message_queue_msg_destroy(&m2);
    krs_message_queue_msg_destroy(&m3);
    krs_message_queue_destroy(&q);
}

void test_mq_pop_timeout_empty(void) {
    MessageQueue_t* q = krs_message_queue_create(16);

    IncomingMessage_t* result = krs_message_queue_pop(q, 50);
    TEST_ASSERT_NULL(result);

    krs_message_queue_destroy(&q);
}

void test_mq_auto_grow(void) {
    MessageQueue_t* q = krs_message_queue_create(2);
    TEST_ASSERT_EQUAL_size_t(2, q->capacity);

    krs_message_queue_push(q, s_make_msg(10, 100));
    krs_message_queue_push(q, s_make_msg(20, 100));
    krs_message_queue_push(q, s_make_msg(30, 100));

    TEST_ASSERT_EQUAL_size_t(3, q->count);
    TEST_ASSERT_TRUE(q->capacity > 2);

    IncomingMessage_t* m1 = krs_message_queue_pop(q, 100);
    IncomingMessage_t* m2 = krs_message_queue_pop(q, 100);
    IncomingMessage_t* m3 = krs_message_queue_pop(q, 100);

    TEST_ASSERT_EQUAL_UINT8(10, m1->data[0]);
    TEST_ASSERT_EQUAL_UINT8(20, m2->data[0]);
    TEST_ASSERT_EQUAL_UINT8(30, m3->data[0]);

    krs_message_queue_msg_destroy(&m1);
    krs_message_queue_msg_destroy(&m2);
    krs_message_queue_msg_destroy(&m3);
    krs_message_queue_destroy(&q);
}

void test_mq_max_capacity_drops_oldest(void) {
    MessageQueue_t* q = krs_message_queue_create(4);
    q->max_capacity = 4;

    krs_message_queue_push(q, s_make_msg(1, 100));
    krs_message_queue_push(q, s_make_msg(2, 100));
    krs_message_queue_push(q, s_make_msg(3, 100));
    krs_message_queue_push(q, s_make_msg(4, 100));
    TEST_ASSERT_EQUAL_size_t(4, q->count);

    krs_message_queue_push(q, s_make_msg(5, 100));
    TEST_ASSERT_EQUAL_size_t(4, q->count);

    IncomingMessage_t* first = krs_message_queue_pop(q, 100);
    TEST_ASSERT_EQUAL_UINT8(2, first->data[0]);

    krs_message_queue_msg_destroy(&first);

    IncomingMessage_t* m;
    while ((m = krs_message_queue_pop(q, 50)) != NULL) {
        krs_message_queue_msg_destroy(&m);
    }
    krs_message_queue_destroy(&q);
}

void test_mq_stop_wakes_consumer(void) {
    MessageQueue_t* q = krs_message_queue_create(16);

    krs_message_queue_stop(q);

    IncomingMessage_t* result = krs_message_queue_pop(q, 1000);
    TEST_ASSERT_NULL(result);

    krs_message_queue_destroy(&q);
}

void test_mq_destroy_frees_remaining(void) {
    MessageQueue_t* q = krs_message_queue_create(16);

    krs_message_queue_push(q, s_make_msg(1, 100));
    krs_message_queue_push(q, s_make_msg(2, 100));
    krs_message_queue_push(q, s_make_msg(3, 100));

    krs_message_queue_destroy(&q);
    TEST_ASSERT_NULL(q);
}

void test_mq_push_null_params(void) {
    MessageQueue_t* q = krs_message_queue_create(16);
    IncomingMessage_t* msg = s_make_msg(1, 100);

    krs_message_queue_push(NULL, msg);
    krs_message_queue_push(q, NULL);

    TEST_ASSERT_EQUAL_size_t(0, q->count);

    krs_message_queue_msg_destroy(&msg);
    krs_message_queue_destroy(&q);
}

void test_mq_pop_null_queue(void) {
    IncomingMessage_t* result = krs_message_queue_pop(NULL, 100);
    TEST_ASSERT_NULL(result);
}

void test_mq_msg_destroy(void) {
    IncomingMessage_t* msg = s_make_msg(0xFF, 9999);
    TEST_ASSERT_NOT_NULL(msg);

    krs_message_queue_msg_destroy(&msg);
    TEST_ASSERT_NULL(msg);

    krs_message_queue_msg_destroy(NULL);
}
