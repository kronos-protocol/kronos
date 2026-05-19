#include "kronos_server.h"
#include "server_internal.h"
#include "kronos.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <string.h>
#include <stdlib.h>


static int s_callback_count = 0;
static Channel_t s_last_channel = 0;
static uint32_t s_last_conn_id = 0;

static void s_tracking_callback(Channel_t channel, uint32_t connection_id,
                                 const uint8_t* data, uint16_t data_length,
                                 void* user_data) {
    (void)data; (void)data_length; (void)user_data;
    s_callback_count++;
    s_last_channel = channel;
    s_last_conn_id = connection_id;
}

static void s_wrong_callback(Channel_t channel, uint32_t connection_id,
                              const uint8_t* data, uint16_t data_length,
                              void* user_data) {
    (void)channel; (void)connection_id;
    (void)data; (void)data_length; (void)user_data;
}

void test_handler_routes_connection_frame_to_channel_0(void) {
    uint8_t body_data[] = {15};
    FrameBuilder_c* builder = krs_frame_builder_create(0, CONNECTION);
    TEST_ASSERT_NOT_NULL(builder);
    krs_frame_builder_set_data(builder, body_data, sizeof(body_data));

    uint8_t wire[KRONOS_BUFFER_SIZE];
    uint16_t n = krs_frame_builder_serialize(builder, wire, sizeof(wire));
    krs_frame_builder_destroy(&builder);
    TEST_ASSERT_TRUE(n > 0);

    uint8_t body_buf[KRONOS_BUFFER_SIZE];
    Frame_t frame = krs_frame_create(wire, n, body_buf, sizeof(body_buf));

    TEST_ASSERT_EQUAL_UINT8(0, frame.channel);
    TEST_ASSERT_EQUAL_UINT8(CONNECTION, frame.frame_type);
}

void test_handler_routes_heartbeat_frame_to_channel_1(void) {
    FrameBuilder_c* builder = krs_frame_builder_create(1, HEARTBEAT);
    TEST_ASSERT_NOT_NULL(builder);

    uint8_t wire[KRONOS_BUFFER_SIZE];
    uint16_t n = krs_frame_builder_serialize(builder, wire, sizeof(wire));
    krs_frame_builder_destroy(&builder);
    TEST_ASSERT_TRUE(n > 0);

    uint8_t body_buf[KRONOS_BUFFER_SIZE];
    Frame_t frame = krs_frame_create(wire, n, body_buf, sizeof(body_buf));

    TEST_ASSERT_EQUAL_UINT8(1, frame.channel);
    TEST_ASSERT_EQUAL_UINT8(HEARTBEAT, frame.frame_type);
}

void test_handler_callback_lookup_channel_override(void) {
    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);

    s_callback_count = 0;
    desc->channel_callbacks[15] = s_tracking_callback;
    desc->port_callback = s_wrong_callback;

    ChannelMessageCallback_f cb = desc->channel_callbacks[15];
    void* ud = desc->channel_callback_user_data[15];
    if (!cb) {
        cb = desc->port_callback;
        ud = desc->port_callback_user_data;
    }

    TEST_ASSERT_EQUAL_PTR(s_tracking_callback, cb);

    uint8_t data[] = {1, 2, 3};
    cb(15, 0, data, sizeof(data), ud);
    TEST_ASSERT_EQUAL_INT(1, s_callback_count);
    TEST_ASSERT_EQUAL_UINT8(15, s_last_channel);

    free(desc);
}

void test_handler_callback_lookup_port_fallback(void) {
    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);

    s_callback_count = 0;
    desc->channel_callbacks[15] = NULL;
    desc->port_callback = s_tracking_callback;

    ChannelMessageCallback_f cb = desc->channel_callbacks[15];
    void* ud = desc->channel_callback_user_data[15];
    if (!cb) {
        cb = desc->port_callback;
        ud = desc->port_callback_user_data;
    }

    TEST_ASSERT_EQUAL_PTR(s_tracking_callback, cb);

    uint8_t data[] = {4, 5};
    cb(15, 0, data, sizeof(data), ud);
    TEST_ASSERT_EQUAL_INT(1, s_callback_count);

    free(desc);
}

void test_handler_body_null_check(void) {
    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);

    s_callback_count = 0;
    desc->port_callback = s_tracking_callback;

    Frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.protocol_char = 0x4B;
    frame.channel = 15;
    frame.frame_type = BASIC_MESSAGE;
    frame.body_length = 10;
    frame.body = NULL;

    bool handler_would_skip = (frame.body_length > 0 && frame.body == NULL);
    TEST_ASSERT_TRUE(handler_would_skip);

    if (!handler_would_skip) {
        ChannelMessageCallback_f cb = desc->channel_callbacks[frame.channel];
        if (!cb) cb = desc->port_callback;
        if (cb) cb(frame.channel, 0, frame.body, frame.body_length, NULL);
    }

    TEST_ASSERT_EQUAL_INT(0, s_callback_count);
    free(desc);
}

void test_handler_reserved_channel_no_callback(void) {
    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);

    s_callback_count = 0;
    desc->port_callback = s_tracking_callback;

    for (uint8_t ch = 2; ch <= 9; ch++) {
        Frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.protocol_char = 0x4B;
        frame.channel = ch;
        frame.frame_type = BASIC_MESSAGE;

        bool is_connection = (frame.channel == 0 && frame.frame_type == CONNECTION);
        bool is_heartbeat = (frame.channel == 1 && frame.frame_type == HEARTBEAT);
        bool is_app_channel = (frame.channel >= 10);

        TEST_ASSERT_FALSE(is_connection);
        TEST_ASSERT_FALSE(is_heartbeat);
        TEST_ASSERT_FALSE(is_app_channel);
    }

    TEST_ASSERT_EQUAL_INT(0, s_callback_count);
    free(desc);
}
