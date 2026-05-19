#include "message_pool_internal.h"
#include "kronos_log.h"

#include <stdlib.h>

#include <windows.h>


#define KRS_MESSAGE_POOL_MAX_FALLBACK 1024u

MessagePool_t* krs_message_pool_create(uint32_t capacity) {
    if (capacity == 0) capacity = 256;

    MessagePool_t* pool = calloc(1, sizeof(MessagePool_t));
    if (!pool) {
        KRS_LOG_ERROR("message_pool", "pool struct allocation failed");
        return NULL;
    }

    pool->storage = calloc(capacity, sizeof(IncomingMessage_t));
    pool->free_stack = malloc(capacity * sizeof(IncomingMessage_t*));
    if (!pool->storage || !pool->free_stack) {
        KRS_LOG_ERROR("message_pool", "pool storage allocation failed for capacity %u", capacity);
        free(pool->storage);
        free(pool->free_stack);
        free(pool);
        return NULL;
    }

    pool->capacity = capacity;
    pool->free_count = capacity;
    InitializeCriticalSection(&pool->lock);

    for (uint32_t i = 0; i < capacity; i++) {
        pool->free_stack[i] = &pool->storage[i];
    }

    return pool;
}

void krs_message_pool_destroy(MessagePool_t** pool) {
    if (!pool || !*pool) return;
    MessagePool_t* p = *pool;
    DeleteCriticalSection(&p->lock);
    free(p->storage);
    free(p->free_stack);
    free(p);
    *pool = NULL;
}

IncomingMessage_t* krs_message_pool_acquire(MessagePool_t* pool) {
    if (!pool) return malloc(sizeof(IncomingMessage_t));

    EnterCriticalSection(&pool->lock);
    if (pool->free_count > 0) {
        pool->free_count--;
        IncomingMessage_t* msg = pool->free_stack[pool->free_count];
        LeaveCriticalSection(&pool->lock);
        return msg;
    }
    LeaveCriticalSection(&pool->lock);

    LONG64 outstanding = InterlockedIncrement64(&pool->outstanding_fallback_count);
    if (outstanding > (LONG64)KRS_MESSAGE_POOL_MAX_FALLBACK) {
        InterlockedDecrement64(&pool->outstanding_fallback_count);
        return NULL;
    }

    IncomingMessage_t* msg = malloc(sizeof(IncomingMessage_t));
    if (!msg) {
        InterlockedDecrement64(&pool->outstanding_fallback_count);
        return NULL;
    }

    InterlockedIncrement64(&pool->fallback_count);
    return msg;
}

uint64_t krs_message_pool_get_fallback_count(const MessagePool_t* pool) {
    if (!pool) return 0;
    return (uint64_t)InterlockedOr64((volatile LONG64*)&pool->fallback_count, 0);
}

void krs_message_pool_release(MessagePool_t* pool, IncomingMessage_t* msg) {
    if (!msg) return;
    if (!pool) {
        free(msg);
        return;
    }

    uintptr_t msg_addr = (uintptr_t)msg;
    uintptr_t pool_start = (uintptr_t)pool->storage;
    uintptr_t pool_end = pool_start + (uintptr_t)pool->capacity * sizeof(IncomingMessage_t);

    if (msg_addr >= pool_start && msg_addr < pool_end) {
        EnterCriticalSection(&pool->lock);
        pool->free_stack[pool->free_count] = msg;
        pool->free_count++;
        LeaveCriticalSection(&pool->lock);
    } else {
        free(msg);
        InterlockedDecrement64(&pool->outstanding_fallback_count);
    }
}
