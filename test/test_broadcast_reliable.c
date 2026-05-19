#include "kronos_server.h"
#include "kronos_client.h"
#include "kronos_network.h"
#include "kronos_ack.h"

#include "server_internal.h"
#include "client_internal.h"
#include "connection_map_internal.h"

#include <unity.h>
#include <windows.h>

static volatile LONG s_failures = 0;
static volatile uint32_t s_failed_conn_id = 0;

static void s_on_failure(uint32_t conn_id, Channel_t channel, uint64_t pid, void* ud) {
    (void)channel; (void)pid; (void)ud;
    InterlockedIncrement(&s_failures);
    s_failed_conn_id = conn_id;
}

void test_reliable_broadcast_per_recipient_failure(void) {
    InterlockedExchange(&s_failures, 0);
    s_failed_conn_id = 0;

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    krs_server_port_manager_port_add(spm, 19504);
    krs_server_set_delivery_failure_callback(spm, 19504, s_on_failure, NULL);
    krs_server_start(spm);

    PortAddress_t srv = krs_network_port_address_create(19504, addr);

    ServerConnection_t* a = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(a);
    ServerConnection_t* b = krs_client_server_connect(srv);
    TEST_ASSERT_NOT_NULL(b);

    TEST_ASSERT_TRUE(krs_client_subscribe(a, 11, 2000).base.valid);
    TEST_ASSERT_TRUE(krs_client_subscribe(b, 11, 2000).base.valid);

    krs_client_start_receive(a);
    Sleep(50);

    uint8_t payload[8] = "ping";
    krs_server_broadcast_reliable(spm, 11, payload, 4);

    // Shorten the auto-created AckTracker on b (the non-receiving client) so that
    // exponential backoff (1+2+4+8+16+32 = 63 multiplier) fits under the 7000ms test
    // budget. With base=100ms, total time-to-failure-callback is ~6.3s.
    UDPSocketDescriptor_t* b_desc = NULL;
    ClientConnection_t* b_target = NULL;
    if (krs_connection_map_acquire(spm->connection_map, b->connection_id, &b_desc, &b_target)) {
        AcquireSRWLockExclusive(&b_target->ack_lock);
        if (b_target->ack_tracker) {
            krs_ack_tracker_set_timeout(b_target->ack_tracker, 100);
        }
        ReleaseSRWLockExclusive(&b_target->ack_lock);
        krs_connection_map_release(b_target);
    }

    Sleep(7000);

    TEST_ASSERT_TRUE(InterlockedAdd(&s_failures, 0) >= 1);

    krs_client_disconnect(&a);
    krs_client_disconnect(&b);
    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);
}
