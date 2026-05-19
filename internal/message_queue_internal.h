#ifndef MESSAGE_QUEUE_INTERNAL_H
#define MESSAGE_QUEUE_INTERNAL_H

#include "kronos_network.h"
#include "kronos_internal.h"

#include <winsock2.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct IncomingMessage IncomingMessage_t;
typedef struct MessageQueue MessageQueue_t;
typedef struct MessagePool MessagePool_t;

struct IncomingMessage {
    uint8_t       data[KRONOS_BUFFER_SIZE];
    uint32_t      data_length;
    PortAddress_t remote_address;
    Port_t        port;
};

struct MessageQueue {
    CRITICAL_SECTION    lock;
    CONDITION_VARIABLE  not_empty;
    IncomingMessage_t** items;
    size_t              head;
    size_t              tail;
    size_t              count;
    size_t              capacity;
    size_t              max_capacity;
    bool                stopped;
    MessagePool_t*      pool;
};

MessageQueue_t* krs_message_queue_create(size_t initial_capacity);
void            krs_message_queue_destroy(MessageQueue_t** queue);
void            krs_message_queue_drain(MessageQueue_t* queue, MessagePool_t* pool);
void            krs_message_queue_push(MessageQueue_t* queue, IncomingMessage_t* msg);
IncomingMessage_t* krs_message_queue_pop(MessageQueue_t* queue, DWORD timeout_ms);
void            krs_message_queue_stop(MessageQueue_t* queue);
void            krs_message_queue_msg_destroy(IncomingMessage_t** msg);
void            krs_message_queue_set_pool(MessageQueue_t* queue, MessagePool_t* pool);

#endif // MESSAGE_QUEUE_INTERNAL_H
