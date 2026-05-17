#include "unity.h"
#include "kronos.h"
#include "kronos_server.h"
#include "kronos_client.h"
#include "kronos_network.h"

#include <windows.h>

static volatile LONG s_disconnect_count = 0;
static volatile LONG s_disconnect_channel_sum = 0;

static void s_on_disconnect(uint32_t connection_id, Channel_t channel, void* ud) {
    (void)connection_id; (void)ud;
    InterlockedIncrement(&s_disconnect_count);
    InterlockedAdd(&s_disconnect_channel_sum, (LONG)channel);
}

void test_disconnect_callback_fires_once_with_channel_zero(void) {
    InterlockedExchange(&s_disconnect_count, 0);
    InterlockedExchange(&s_disconnect_channel_sum, 0);

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    krs_server_port_manager_port_add(spm, 19505);
    krs_server_set_disconnect_callback(spm, 19505, s_on_disconnect, NULL);
    krs_server_start(spm);

    PortAddress_t srv = krs_network_port_address_create(19505, addr);
    ServerConnection_t* conn = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(conn);
    krs_client_start_receive(conn);

    TEST_ASSERT_TRUE(krs_client_subscribe(conn, 10, 2000).base.valid);
    TEST_ASSERT_TRUE(krs_client_subscribe(conn, 20, 2000).base.valid);
    TEST_ASSERT_TRUE(krs_client_subscribe(conn, 30, 2000).base.valid);

    krs_client_disconnect(&conn);
    Sleep(300);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, InterlockedAdd(&s_disconnect_count, 0),
        "disconnect callback should fire exactly once per unique connection");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, InterlockedAdd(&s_disconnect_channel_sum, 0),
        "disconnect callback channel parameter should be 0 (lifecycle channel)");

    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
}

static SRWLOCK s_stop_sweep_lock;
static volatile LONG s_stop_sweep_count;
static volatile LONG s_stop_sweep_channel_sum;
static uint32_t s_stop_sweep_ids[16];

static void s_on_stop_disconnect(uint32_t connection_id, Channel_t channel, void* ud) {
    (void)ud;
    LONG idx = InterlockedIncrement(&s_stop_sweep_count) - 1;
    InterlockedAdd(&s_stop_sweep_channel_sum, (LONG)channel);
    if (idx >= 0 && idx < 16) {
        AcquireSRWLockExclusive(&s_stop_sweep_lock);
        s_stop_sweep_ids[idx] = connection_id;
        ReleaseSRWLockExclusive(&s_stop_sweep_lock);
    }
}

void test_disconnect_callback_fires_on_server_stop_for_active_clients(void) {
    InitializeSRWLock(&s_stop_sweep_lock);
    InterlockedExchange(&s_stop_sweep_count, 0);
    InterlockedExchange(&s_stop_sweep_channel_sum, 0);
    memset(s_stop_sweep_ids, 0, sizeof(s_stop_sweep_ids));

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    krs_server_port_manager_port_add(spm, 19506);
    krs_server_set_disconnect_callback(spm, 19506, s_on_stop_disconnect, NULL);
    krs_server_start(spm);

    PortAddress_t srv = krs_network_port_address_create(19506, addr);
    ServerConnection_t* c1 = krs_client_server_connect(srv);
    ServerConnection_t* c2 = krs_client_server_connect(srv);
    ServerConnection_t* c3 = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT_NOT_NULL(c2);
    TEST_ASSERT_NOT_NULL(c3);

    Sleep(100);

    krs_server_stop(spm);

    TEST_ASSERT_EQUAL_INT_MESSAGE(3, InterlockedAdd(&s_stop_sweep_count, 0),
        "disconnect callback should fire once per active client at server stop");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, InterlockedAdd(&s_stop_sweep_channel_sum, 0),
        "disconnect callback channel parameter should be 0 for every client");

    AcquireSRWLockExclusive(&s_stop_sweep_lock);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_NOT_EQUAL(0, s_stop_sweep_ids[i]);
        for (int j = i + 1; j < 3; j++) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(s_stop_sweep_ids[i], s_stop_sweep_ids[j],
                "disconnect callback should report distinct connection_ids");
        }
    }
    ReleaseSRWLockExclusive(&s_stop_sweep_lock);

    krs_client_disconnect(&c1);
    krs_client_disconnect(&c2);
    krs_client_disconnect(&c3);
    krs_server_port_manager_destroy(&spm);
}
