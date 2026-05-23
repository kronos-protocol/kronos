#include "kronos_server.h"
#include "kronos_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef enum { MODE_ECHO, MODE_SINK } ServerMode_e;

static ServerPortManager_t* s_spm = NULL;
static ServerMode_e s_mode = MODE_ECHO;
static volatile LONG s_rx_count = 0;
static volatile LONG s_tx_count = 0;
static volatile bool s_running = true;

static void s_on_connect(uint32_t connection_id, Channel_t channel, void* user_data) {
    (void)user_data;
    printf("[BENCH] Client connected: id=%u ch=%u\n", connection_id, channel);
}

static void s_on_disconnect(uint32_t connection_id, Channel_t channel, void* user_data) {
    (void)user_data;
    printf("[BENCH] Client disconnected: id=%u ch=%u\n", connection_id, channel);
}

static void s_on_message(Channel_t channel, uint32_t connection_id,
                         const uint8_t* data, uint16_t data_length,
                         void* user_data) {
    (void)user_data;

    InterlockedIncrement(&s_rx_count);

    if (s_mode == MODE_ECHO && s_spm && connection_id > 0) {
        krs_server_send(s_spm, connection_id, channel, data, data_length, false);
        InterlockedIncrement(&s_tx_count);
    }
}

static DWORD WINAPI s_stats_ticker(LPVOID param) {
    (void)param;
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    LONG prev_rx = 0;
    double prev_time = 0.0;

    while (s_running) {
        Sleep(1000);
        if (!s_running) break;

        QueryPerformanceCounter(&now);
        double elapsed = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        double interval = elapsed - prev_time;

        LONG cur_rx = InterlockedOr(&s_rx_count, 0);
        LONG delta_rx = cur_rx - prev_rx;

        double rate_msg = (interval > 0) ? (double)delta_rx / interval : 0.0;

        printf("[%5.1fs] rx: %6.0f msg/s | total rx: %ld | tx: %ld\n",
               elapsed, rate_msg, (long)cur_rx, (long)InterlockedOr(&s_tx_count, 0));

        prev_rx = cur_rx;
        prev_time = elapsed;
    }
    return 0;
}

static void s_print_usage(void) {
    printf("Usage: bench_server [options]\n");
    printf("  --port PORT    Listen port (default: 9001)\n");
    printf("  --mode MODE    echo = echo all messages (default, for latency tests)\n");
    printf("                 sink = receive and discard (for throughput tests)\n");
    printf("  -h, --help     Show this help\n");
}

int main(int argc, char* argv[]) {
    uint16_t port = 9001;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "echo") == 0) s_mode = MODE_ECHO;
            else if (strcmp(argv[i], "sink") == 0) s_mode = MODE_SINK;
            else { s_print_usage(); return 1; }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            s_print_usage();
            return 0;
        }
    }

    printf("=== Kronos Benchmark Server ===\n");
    printf("Port: %u  |  Mode: %s\n\n", port, s_mode == MODE_ECHO ? "echo" : "sink");

    Address_t addr = krs_network_address_ipv4_create("0.0.0.0");
    s_spm = krs_server_port_manager_create(addr);
    if (!s_spm) {
        printf("Failed to create server port manager\n");
        return 1;
    }

    Void_r port_r = krs_server_port_manager_port_add(s_spm, port);
    if (!port_r.base.valid) {
        printf("Failed to add port %u: %s\n", port,
               port_r.base.error_message ? port_r.base.error_message : "unknown");
        krs_server_port_manager_destroy(&s_spm);
        return 1;
    }
    krs_server_set_port_callback(s_spm, port, s_on_message, NULL);
    krs_server_set_connect_callback(s_spm, port, s_on_connect, NULL);
    krs_server_set_disconnect_callback(s_spm, port, s_on_disconnect, NULL);

    Void_r r = krs_server_start(s_spm);
    if (!r.base.valid) {
        printf("Start failed: %s\n", r.base.error_message ? r.base.error_message : "unknown");
        krs_server_port_manager_destroy(&s_spm);
        return 1;
    }

    HANDLE ticker = CreateThread(NULL, 0, s_stats_ticker, NULL, 0, NULL);
    printf("Running. Press Enter to stop.\n\n");
    getchar();

    s_running = false;
    if (ticker) { WaitForSingleObject(ticker, 2000); CloseHandle(ticker); }

    printf("\n=== Final ===\n");
    printf("Total received: %ld messages\n", (long)s_rx_count);
    printf("Total echoed:   %ld messages\n", (long)s_tx_count);

    krs_server_stop(s_spm);
    krs_server_port_manager_destroy(&s_spm);
    printf("Server stopped.\n");
    return 0;
}
