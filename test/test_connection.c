#include "kronos_server.h"
#include "server_internal.h"
#include "kronos_client.h"
#include "client_internal.h"
#include "kronos.h"
#include "network_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <string.h>
#include <stdlib.h>


static void s_dummy_callback(Channel_t channel, ChannelType_e channel_type,
                              uint32_t connection_id, const uint8_t* data,
                              uint16_t data_length, void* user_data) {
    (void)channel; (void)channel_type; (void)connection_id;
    (void)data; (void)data_length; (void)user_data;
}

static ServerPortManager_t* s_make_spm(void) {
    Address_t addr;
    memset(&addr, 0, sizeof(addr));
    return krs_server_port_manager_create(addr);
}

void test_spm_create_destroy(void) {
    ServerPortManager_t* spm = s_make_spm();
    TEST_ASSERT_NOT_NULL(spm);
    krs_server_port_manager_destroy(&spm);
    TEST_ASSERT_NULL(spm);
}

void test_spm_create_malloc_failure(void) {
    mock_malloc_fail_next();
    Address_t addr;
    memset(&addr, 0, sizeof(addr));
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    TEST_ASSERT_NULL(spm);
}

void test_set_port_callback_null_spm(void) {
    Void_r r = krs_server_set_port_callback(NULL, 8080, s_dummy_callback, NULL);
    TEST_ASSERT_FALSE(r.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_NULL_POINTER, r.base.error_code);
}

void test_set_channel_callback_reserved_channel(void) {
    ServerPortManager_t* spm = s_make_spm();
    TEST_ASSERT_NOT_NULL(spm);

    for (uint8_t ch = 0; ch < 10; ch++) {
        Void_r r = krs_server_set_channel_callback(spm, 8080, ch, s_dummy_callback, NULL);
        TEST_ASSERT_FALSE(r.base.valid);
        TEST_ASSERT_EQUAL_INT(KRS_ERR_INVALID_PARAMETER, r.base.error_code);
    }

    krs_server_port_manager_destroy(&spm);
}

void test_set_channel_callback_null_spm(void) {
    Void_r r = krs_server_set_channel_callback(NULL, 8080, 10, s_dummy_callback, NULL);
    TEST_ASSERT_FALSE(r.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_NULL_POINTER, r.base.error_code);
}

void test_set_channel_callback_no_descriptor(void) {
    ServerPortManager_t* spm = s_make_spm();
    TEST_ASSERT_NOT_NULL(spm);

    Void_r r = krs_server_set_channel_callback(spm, 9999, 10, s_dummy_callback, NULL);
    TEST_ASSERT_FALSE(r.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_NOT_INITIALIZED, r.base.error_code);

    krs_server_port_manager_destroy(&spm);
}

void test_broadcast_null_params(void) {
    uint8_t data[4] = {1, 2, 3, 4};
    krs_server_broadcast(NULL, 10, data, sizeof(data));
    krs_server_broadcast(NULL, 10, NULL, 4);
    ServerPortManager_t* spm = s_make_spm();
    krs_server_broadcast(spm, 10, NULL, 4);
    krs_server_port_manager_destroy(&spm);
}

void test_broadcast_except_null_params(void) {
    uint8_t data[4] = {1, 2, 3, 4};
    krs_server_broadcast_except(NULL, 10, 0, data, sizeof(data));
    ServerPortManager_t* spm = s_make_spm();
    krs_server_broadcast_except(spm, 10, 0, NULL, 4);
    krs_server_port_manager_destroy(&spm);
}

void test_client_disconnect_null(void) {
    krs_client_disconnect(NULL);
    ServerConnection_t* conn = NULL;
    krs_client_disconnect(&conn);
}

void test_client_send_null_conn(void) {
    uint8_t data[4] = {0};
    Void_r r = krs_client_send(NULL, 10, data, sizeof(data), false);
    TEST_ASSERT_FALSE(r.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_NULL_POINTER, r.base.error_code);
}

void test_client_send_null_data(void) {
    ServerConnection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.connected = true;
    conn.socket = INVALID_SOCKET;
    Void_r r = krs_client_send(&conn, 10, NULL, 4, false);
    TEST_ASSERT_FALSE(r.base.valid);
    TEST_ASSERT_EQUAL_INT(KRS_ERR_NULL_POINTER, r.base.error_code);
}

void test_handle_connection_frame_null_params(void) {
    krs_server_handle_connection_frame(NULL, NULL, NULL, NULL);
}

void test_handle_heartbeat_frame_null_params(void) {
    krs_server_handle_heartbeat_frame(NULL, NULL, NULL);
}

static Frame_t s_build_frame(uint8_t channel, uint8_t frame_type,
                              uint8_t* body, uint16_t body_length) {
    Frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.protocol_char = 0x4B;
    frame.channel = channel;
    frame.frame_type = frame_type;
    frame.body = body;
    frame.body_length = body_length;
    return frame;
}

void test_handle_connection_frame_creates_connection(void) {
    krs_wsa_init();

    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);
    for (uint32_t ch = 0; ch <= MAX_CHANNEL_NUMBER; ch++) {
        InitializeSRWLock(&desc->channel_states[ch].channel_lock);
    }
    desc->udp_socket_ref = INVALID_SOCKET;

    ServerPortManager_t* spm = calloc(1, sizeof(ServerPortManager_t));
    TEST_ASSERT_NOT_NULL(spm);

    uint8_t body_data[] = {15};
    Frame_t frame = s_build_frame(0, CONNECTION, body_data, sizeof(body_data));

    Address_t ip = krs_network_address_ipv4_create("10.0.0.1");
    PortAddress_t remote_addr = krs_network_port_address_create(5000, ip);

    krs_server_handle_connection_frame(spm, desc, &frame, &remote_addr);

    TEST_ASSERT_NOT_NULL(desc->channel_states[15].connections);
    TEST_ASSERT_EQUAL_UINT32(1, krs_array_length(desc->channel_states[15].connections));

    ClientConnection_t* conn = KRS_ARRAY_GET(desc->channel_states[15].connections, 0, ClientConnection_t);
    TEST_ASSERT_NOT_NULL(conn);
    TEST_ASSERT_TRUE(conn->connection_id > 0);
    TEST_ASSERT_TRUE(krs_network_port_address_equals(&conn->remote_address, &remote_addr));

    free(conn);
    krs_array_destroy(&desc->channel_states[15].connections);
    free(spm);
    free(desc);
    krs_wsa_cleanup();
}

void test_handle_heartbeat_updates_timestamp(void) {
    krs_wsa_init();

    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);

    Address_t ip = krs_network_address_ipv4_create("10.0.0.1");
    PortAddress_t remote_addr = krs_network_port_address_create(5000, ip);

    ClientConnection_t* conn = malloc(sizeof(ClientConnection_t));
    TEST_ASSERT_NOT_NULL(conn);
    conn->connection_id = 42;
    conn->remote_address = remote_addr;
    conn->last_heartbeat_ms = 0;

    desc->channel_states[10].connections = krs_array_create(4);
    krs_array_push(desc->channel_states[10].connections, conn);

    Frame_t frame = s_build_frame(1, HEARTBEAT, NULL, 0);
    krs_server_handle_heartbeat_frame(desc, &frame, &remote_addr);

    TEST_ASSERT_TRUE(conn->last_heartbeat_ms > 0);

    free(conn);
    krs_array_destroy(&desc->channel_states[10].connections);
    free(desc);
    krs_wsa_cleanup();
}

void test_handle_heartbeat_unknown_addr_no_update(void) {
    krs_wsa_init();

    UDPSocketDescriptor_t* desc = calloc(1, sizeof(UDPSocketDescriptor_t));
    TEST_ASSERT_NOT_NULL(desc);
    InitializeSRWLock(&desc->state_lock);

    Address_t ip1 = krs_network_address_ipv4_create("10.0.0.1");
    PortAddress_t remote_addr = krs_network_port_address_create(5000, ip1);

    ClientConnection_t* conn = malloc(sizeof(ClientConnection_t));
    TEST_ASSERT_NOT_NULL(conn);
    conn->connection_id = 42;
    conn->remote_address = remote_addr;
    conn->last_heartbeat_ms = 0;

    desc->channel_states[10].connections = krs_array_create(4);
    krs_array_push(desc->channel_states[10].connections, conn);

    Address_t ip2 = krs_network_address_ipv4_create("10.0.0.2");
    PortAddress_t different_addr = krs_network_port_address_create(6000, ip2);

    Frame_t frame = s_build_frame(1, HEARTBEAT, NULL, 0);
    krs_server_handle_heartbeat_frame(desc, &frame, &different_addr);

    TEST_ASSERT_EQUAL_UINT64(0, conn->last_heartbeat_ms);

    free(conn);
    krs_array_destroy(&desc->channel_states[10].connections);
    free(desc);
    krs_wsa_cleanup();
}

void test_set_channel_callback_and_verify(void) {
    krs_wsa_init();

    ServerPortManager_t* spm = s_make_spm();
    TEST_ASSERT_NOT_NULL(spm);

    krs_server_port_manager_port_add(spm, 18888);

    int user_data = 99;
    Void_r r = krs_server_set_channel_callback(spm, 18888, 15, s_dummy_callback, &user_data);
    TEST_ASSERT_TRUE(r.base.valid);

    PortTableLookup_r lookup = krs_lib_port_table_lookup(spm->port_table, 18888);
    TEST_ASSERT_TRUE(lookup.exists);

    UDPSocketDescriptor_t* desc = lookup.socket_handler;
    TEST_ASSERT_EQUAL_PTR(s_dummy_callback, desc->channel_callbacks[15]);
    TEST_ASSERT_EQUAL_PTR(&user_data, desc->channel_callback_user_data[15]);

    krs_server_port_manager_destroy(&spm);
    krs_wsa_cleanup();
}

void test_set_port_callback_and_verify(void) {
    krs_wsa_init();

    ServerPortManager_t* spm = s_make_spm();
    TEST_ASSERT_NOT_NULL(spm);

    krs_server_port_manager_port_add(spm, 18889);

    int user_data = 77;
    Void_r r = krs_server_set_port_callback(spm, 18889, s_dummy_callback, &user_data);
    TEST_ASSERT_TRUE(r.base.valid);

    PortTableLookup_r lookup = krs_lib_port_table_lookup(spm->port_table, 18889);
    TEST_ASSERT_TRUE(lookup.exists);

    UDPSocketDescriptor_t* desc = lookup.socket_handler;
    TEST_ASSERT_EQUAL_PTR(s_dummy_callback, desc->port_callback);
    TEST_ASSERT_EQUAL_PTR(&user_data, desc->port_callback_user_data);

    krs_server_port_manager_destroy(&spm);
    krs_wsa_cleanup();
}
