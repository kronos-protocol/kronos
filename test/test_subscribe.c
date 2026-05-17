#include "kronos.h"
#include "kronos_server.h"
#include "kronos_client.h"
#include "kronos_network.h"
#include "kronos_log.h"

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

static volatile LONG s_received_ch11 = 0;
static volatile LONG s_received_ch12 = 0;

static void s_on_msg(Channel_t channel, uint32_t conn_id,
                     const uint8_t* data, uint16_t len, void* ud) {
    (void)conn_id; (void)data; (void)len; (void)ud;
    if (channel == 11) InterlockedIncrement(&s_received_ch11);
    if (channel == 12) InterlockedIncrement(&s_received_ch12);
}

void test_subscribe_filters_unsubscribed_channels(void) {
    InterlockedExchange(&s_received_ch11, 0);
    InterlockedExchange(&s_received_ch12, 0);

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    krs_server_port_manager_port_add(spm, 19501);
    krs_server_set_channel_callback(spm, 19501, 11, s_on_msg, NULL);
    krs_server_set_channel_callback(spm, 19501, 12, s_on_msg, NULL);
    krs_server_start(spm);

    PortAddress_t srv = krs_network_port_address_create(19501, addr);
    ServerConnection_t* conn = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(conn);

    Void_r sub = krs_client_subscribe(conn, 11, 2000);
    TEST_ASSERT_TRUE(sub.base.valid);

    krs_client_start_receive(conn);

    uint8_t payload[8] = "hello";
    krs_client_send(conn, 11, payload, 5, false);
    krs_client_send(conn, 12, payload, 5, false);

    Sleep(300);

    TEST_ASSERT_EQUAL_INT(1, InterlockedAdd(&s_received_ch11, 0));
    TEST_ASSERT_EQUAL_INT(0, InterlockedAdd(&s_received_ch12, 0));

    krs_client_disconnect(&conn);
    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
}

void test_unsubscribe_stops_delivery(void) {
    InterlockedExchange(&s_received_ch11, 0);

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    krs_server_port_manager_port_add(spm, 19502);
    krs_server_set_channel_callback(spm, 19502, 11, s_on_msg, NULL);
    krs_server_start(spm);

    PortAddress_t srv = krs_network_port_address_create(19502, addr);
    ServerConnection_t* conn = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(conn);

    Void_r sub = krs_client_subscribe(conn, 11, 2000);
    TEST_ASSERT_TRUE(sub.base.valid);

    krs_client_start_receive(conn);

    uint8_t payload[8] = "hi";
    krs_client_send(conn, 11, payload, 2, false);
    Sleep(200);
    TEST_ASSERT_EQUAL_INT(1, InterlockedAdd(&s_received_ch11, 0));

    krs_client_unsubscribe(conn, 11);
    Sleep(100);
    krs_client_send(conn, 11, payload, 2, false);
    Sleep(200);
    TEST_ASSERT_EQUAL_INT(1, InterlockedAdd(&s_received_ch11, 0));

    krs_client_disconnect(&conn);
    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
}
