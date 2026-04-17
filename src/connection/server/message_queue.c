#include "kronos_server.h"
#include "message_queue_internal.h"
#include "message_pool_internal.h"

#include <stdlib.h>
#include <string.h>


MessageQueue_t* krs_message_queue_create(size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 64;

    MessageQueue_t* q = calloc(1, sizeof(MessageQueue_t));
    if (!q) return NULL;

    q->items = malloc(initial_capacity * sizeof(IncomingMessage_t*));
    if (!q->items) {
        free(q);
        return NULL;
    }

    q->capacity = initial_capacity;
    q->max_capacity = 65536;
    InitializeCriticalSection(&q->lock);
    InitializeConditionVariable(&q->not_empty);
    return q;
}

void krs_message_queue_destroy(MessageQueue_t** queue) {
    if (!queue || !*queue) return;
    MessageQueue_t* q = *queue;
    DeleteCriticalSection(&q->lock);
    for (size_t i = 0; i < q->count; i++) {
        size_t idx = (q->head + i) % q->capacity;
        krs_message_pool_release(q->pool, q->items[idx]);
    }
    free(q->items);
    free(q);
    *queue = NULL;
}

void krs_message_queue_push(MessageQueue_t* queue, IncomingMessage_t* msg) {
    if (!queue || !msg) return;

    EnterCriticalSection(&queue->lock);

    if (queue->count == queue->capacity) {
        if (queue->capacity >= queue->max_capacity) {
            IncomingMessage_t* oldest = queue->items[queue->head];
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;
            krs_message_pool_release(queue->pool, oldest);
        } else {
            size_t new_cap = queue->capacity * 2;
            if (new_cap > queue->max_capacity) new_cap = queue->max_capacity;
            IncomingMessage_t** new_items = malloc(new_cap * sizeof(IncomingMessage_t*));
            if (!new_items) {
                LeaveCriticalSection(&queue->lock);
                krs_message_pool_release(queue->pool, msg);
                return;
            }
            for (size_t i = 0; i < queue->count; i++) {
                new_items[i] = queue->items[(queue->head + i) % queue->capacity];
            }
            free(queue->items);
            queue->items = new_items;
            queue->head = 0;
            queue->tail = queue->count;
            queue->capacity = new_cap;
        }
    }

    queue->items[queue->tail] = msg;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    WakeConditionVariable(&queue->not_empty);
    LeaveCriticalSection(&queue->lock);
}

IncomingMessage_t* krs_message_queue_pop(MessageQueue_t* queue, DWORD timeout_ms) {
    if (!queue) return NULL;

    EnterCriticalSection(&queue->lock);

    while (queue->count == 0 && !queue->stopped) {
        if (!SleepConditionVariableCS(&queue->not_empty, &queue->lock, timeout_ms)) {
            LeaveCriticalSection(&queue->lock);
            return NULL;
        }
    }

    if (queue->count == 0) {
        LeaveCriticalSection(&queue->lock);
        return NULL;
    }

    IncomingMessage_t* msg = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    LeaveCriticalSection(&queue->lock);
    return msg;
}

void krs_message_queue_stop(MessageQueue_t* queue) {
    if (!queue) return;
    EnterCriticalSection(&queue->lock);
    queue->stopped = true;
    WakeAllConditionVariable(&queue->not_empty);
    LeaveCriticalSection(&queue->lock);
}

void krs_message_queue_msg_destroy(IncomingMessage_t** msg) {
    if (!msg || !*msg) return;
    free(*msg);
    *msg = NULL;
}

void krs_message_queue_drain(MessageQueue_t* queue, MessagePool_t* pool) {
    if (!queue) return;
    EnterCriticalSection(&queue->lock);
    for (size_t i = 0; i < queue->count; i++) {
        size_t idx = (queue->head + i) % queue->capacity;
        krs_message_pool_release(pool, queue->items[idx]);
        queue->items[idx] = NULL;
    }
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    LeaveCriticalSection(&queue->lock);
}

void krs_message_queue_set_pool(MessageQueue_t* queue, MessagePool_t* pool) {
    if (!queue) return;
    queue->pool = pool;
}
