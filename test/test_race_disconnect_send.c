#include "kronos.h"
#include "kronos_server.h"
#include "kronos_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>


#define RACE_PORT 29999
#define RACE_DURATION_MS 2000

static volatile bool s_stop = false;

static int s_resolve_duration_ms(int argc, char** argv) {
    if (argc >= 2) {
        int v = atoi(argv[1]);
        if (v > 0) return v;
    }
    const char* env = getenv("KRONOS_RACE_DURATION_MS");
    if (env) {
        int v = atoi(env);
        if (v > 0) return v;
    }
    return RACE_DURATION_MS;
}

static void s_on_msg(Channel_t channel, uint32_t conn_id,
                     const uint8_t* data, uint16_t len, void* ud) {
    (void)channel; (void)conn_id; (void)data; (void)len; (void)ud;
}

static DWORD WINAPI s_client_worker(LPVOID param) {
    (void)param;
    PortAddress_t sa = krs_network_port_address_create(
        RACE_PORT, krs_network_address_ipv4_create("127.0.0.1"));

    while (!s_stop) {
        ServerConnection_t* c = krs_client_server_connect(sa);
        if (!c) continue;

        Void_r sub_r = krs_client_subscribe(c, 10, 2000);
        if (!sub_r.base.valid) {
            krs_client_disconnect(&c);
            continue;
        }

        krs_client_set_callback(c, s_on_msg, NULL);
        krs_client_start_receive(c);

        uint8_t payload[64];
        memset(payload, 0xAB, sizeof(payload));
        for (int i = 0; i < 5 && !s_stop; i++) {
            krs_client_send(c, 10, payload, sizeof(payload), true);
            Sleep(1);
        }

        krs_client_disconnect(&c);
    }
    return 0;
}

int main(int argc, char** argv) {
    int duration_ms = s_resolve_duration_ms(argc, argv);

    Address_t addr = krs_network_address_ipv4_create("0.0.0.0");
    ServerPortManager_t* spm = krs_server_port_manager_create(addr);
    if (!spm) {
        fprintf(stderr, "spm create failed\n");
        return 1;
    }

    Void_r pr = krs_server_port_manager_port_add(spm, RACE_PORT);
    if (!pr.base.valid) {
        fprintf(stderr, "port_add failed\n");
        return 1;
    }
    krs_server_set_port_callback(spm, RACE_PORT, s_on_msg, NULL);
    krs_server_start(spm);

    printf("race test running for %d ms with 4 workers\n", duration_ms);
    fflush(stdout);

    HANDLE workers[4];
    for (int i = 0; i < 4; i++) {
        workers[i] = CreateThread(NULL, 0, s_client_worker, NULL, 0, NULL);
    }

    Sleep(duration_ms);
    s_stop = true;

    WaitForMultipleObjects(4, workers, TRUE, 5000);
    for (int i = 0; i < 4; i++) CloseHandle(workers[i]);

    krs_server_stop(spm);
    krs_server_port_manager_destroy(&spm);

    printf("race test completed without crash\n");
    return 0;
}
