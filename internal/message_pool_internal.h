#ifndef MESSAGE_POOL_INTERNAL_H
#define MESSAGE_POOL_INTERNAL_H

#include "message_queue_internal.h"
#include <winsock2.h>
#include <stdint.h>

typedef struct MessagePool MessagePool_t;

struct MessagePool {
    IncomingMessage_t* storage;
    IncomingMessage_t** free_stack;
    uint32_t free_count;
    uint32_t capacity;
    CRITICAL_SECTION lock;
    volatile LONG64 fallback_count;
};

MessagePool_t* krs_message_pool_create(uint32_t capacity);
void krs_message_pool_destroy(MessagePool_t** pool);
IncomingMessage_t* krs_message_pool_acquire(MessagePool_t* pool);
void krs_message_pool_release(MessagePool_t* pool, IncomingMessage_t* msg);
uint64_t krs_message_pool_get_fallback_count(const MessagePool_t* pool);

#endif // MESSAGE_POOL_INTERNAL_H
