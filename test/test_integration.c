#include "kronos_server.h"
#include "kronos_client.h"
#include "server_internal.h"
#include "client_internal.h"
#include "network_internal.h"
#include "kronos.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <string.h>
#include <windows.h>


static volatile int s_integ_received = 0;
static uint8_t s_integ_data[64] = {0};
static uint16_t s_integ_data_len = 0;

static void s_integ_callback(Channel_t channel, ChannelType_e channel_type,
                              uint32_t connection_id, const uint8_t* data,
                              uint16_t data_length, void* user_data) {
    (void)channel; (void)channel_type; (void)connection_id; (void)user_data;
    if (data_length <= sizeof(s_integ_data)) {
        memcpy(s_integ_data, data, data_length);
        s_integ_data_len = data_length;
    }
    s_integ_received++;
}

void test_integration_server_client_roundtrip(void) {
    s_integ_received = 0;
    memset(s_integ_data, 0, sizeof(s_integ_data));
    s_integ_data_len = 0;

    krs_wsa_init();

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    TEST_ASSERT_NOT_NULL(spm);

    krs_server_port_manager_port_add(spm, 19999);

    Void_r cb_r = krs_server_set_port_callback(spm, 19999, s_integ_callback, NULL);
    TEST_ASSERT_TRUE(cb_r.base.valid);

    Void_r start_r = krs_server_start(spm);
    TEST_ASSERT_TRUE(start_r.base.valid);

    PortAddress_t server_addr = krs_network_port_address_create(19999, addr);
    ServerConnection_t* conn = krs_client_server_connect(server_addr);
    TEST_ASSERT_NOT_NULL(conn);
    TEST_ASSERT_TRUE(conn->connection_id > 0);

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    Void_r send_r = krs_client_send(conn, 10, payload, sizeof(payload), false);
    TEST_ASSERT_TRUE(send_r.base.valid);

    Sleep(200);

    TEST_ASSERT_EQUAL_INT(1, s_integ_received);
    TEST_ASSERT_EQUAL_UINT16(4, s_integ_data_len);
    TEST_ASSERT_EQUAL_UINT8(0xDE, s_integ_data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, s_integ_data[3]);

    krs_client_disconnect(&conn);
    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
    krs_wsa_cleanup();
}

static volatile int s_client_received = 0;
static uint8_t s_client_data[64] = {0};
static uint16_t s_client_data_len = 0;

static void s_client_callback(Channel_t channel, ChannelType_e channel_type,
                               uint32_t connection_id, const uint8_t* data,
                               uint16_t data_length, void* user_data) {
    (void)channel; (void)channel_type; (void)connection_id; (void)user_data;
    if (data_length <= sizeof(s_client_data)) {
        memcpy(s_client_data, data, data_length);
        s_client_data_len = data_length;
    }
    s_client_received++;
}

void test_integration_server_sends_to_client(void) {
    s_integ_received = 0;
    s_client_received = 0;
    memset(s_integ_data, 0, sizeof(s_integ_data));
    memset(s_client_data, 0, sizeof(s_client_data));

    krs_wsa_init();

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    TEST_ASSERT_NOT_NULL(spm);
    krs_server_port_manager_port_add(spm, 19998);
    krs_server_set_port_callback(spm, 19998, s_integ_callback, NULL);
    krs_server_start(spm);

    PortAddress_t server_addr = krs_network_port_address_create(19998, addr);
    ServerConnection_t* conn = krs_client_server_connect(server_addr);
    TEST_ASSERT_NOT_NULL(conn);

    krs_client_set_callback(conn, s_client_callback, NULL);
    Void_r recv_r = krs_client_start_receive(conn);
    TEST_ASSERT_TRUE(recv_r.base.valid);

    uint8_t payload[] = {0xCA, 0xFE};
    krs_client_send(conn, 10, payload, sizeof(payload), false);
    Sleep(200);

    TEST_ASSERT_EQUAL_INT(1, s_integ_received);

    uint8_t reply[] = {0xBE, 0xEF, 0x42};
    krs_server_send(spm, conn->connection_id, 10, reply, sizeof(reply), false);
    Sleep(200);

    TEST_ASSERT_EQUAL_INT(1, s_client_received);
    TEST_ASSERT_EQUAL_UINT16(3, s_client_data_len);
    TEST_ASSERT_EQUAL_UINT8(0xBE, s_client_data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x42, s_client_data[2]);

    krs_client_disconnect(&conn);
    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
    krs_wsa_cleanup();
}

static volatile int s_connect_count = 0;
static volatile int s_disconnect_count = 0;
static uint32_t s_connected_id = 0;

static void s_on_connect(uint32_t connection_id, Channel_t channel, void* user_data) {
    (void)channel; (void)user_data;
    s_connect_count++;
    s_connected_id = connection_id;
}

static void s_on_disconnect(uint32_t connection_id, Channel_t channel, void* user_data) {
    (void)connection_id; (void)channel; (void)user_data;
    s_disconnect_count++;
}

void test_integration_connect_disconnect_lifecycle(void) {
    s_connect_count = 0;
    s_disconnect_count = 0;
    s_connected_id = 0;

    krs_wsa_init();

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    TEST_ASSERT_NOT_NULL(spm);
    krs_server_port_manager_port_add(spm, 19997);
    krs_server_set_port_callback(spm, 19997, s_integ_callback, NULL);
    krs_server_set_connect_callback(spm, 19997, s_on_connect, NULL);
    krs_server_set_disconnect_callback(spm, 19997, s_on_disconnect, NULL);
    krs_server_start(spm);

    PortAddress_t server_addr = krs_network_port_address_create(19997, addr);
    ServerConnection_t* conn = krs_client_server_connect(server_addr);
    TEST_ASSERT_NOT_NULL(conn);

    Sleep(200);
    TEST_ASSERT_EQUAL_INT(1, s_connect_count);
    TEST_ASSERT_TRUE(s_connected_id > 0);
    TEST_ASSERT_EQUAL_UINT32(conn->connection_id, s_connected_id);

    krs_client_disconnect(&conn);
    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
    krs_wsa_cleanup();
}
