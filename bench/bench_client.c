#include "kronos_client.h"
#include "kronos_network.h"
#include "client_internal.h"
#include "bench_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef enum {
    TEST_LATENCY,
    TEST_THROUGHPUT,
    TEST_THROUGHPUT_ACK,
    TEST_FRAGMENT,
    TEST_CONNECT
} TestMode_e;

static BenchStats_t* s_stats = NULL;
static volatile LONG s_echo_received = 0;

static void s_on_echo(Channel_t channel, uint32_t connection_id,
                      const uint8_t* data, uint16_t data_length,
                      void* user_data) {
    (void)channel; (void)connection_id; (void)user_data;

    if (data_length >= BENCH_MSG_HEADER_SIZE && s_stats) {
        uint64_t seq, original_ts;
        bench_msg_decode(data, &seq, &original_ts);
        double rtt = (double)(bench_time_us() - original_ts);
        bench_stats_record_latency(s_stats, rtt);
    }
    InterlockedIncrement(&s_echo_received);
}

/* ---- Wait for echo with spin + yield ---- */
static inline void s_wait_for_echo(LONG expected) {
    while (InterlockedOr(&s_echo_received, 0) < expected) {
        YieldProcessor();
    }
}

/* ---- Test: Sequential Echo Latency ---- */
static void s_run_latency(ServerConnection_t* conn, uint32_t count, uint32_t msg_size) {
    uint8_t* buf = calloc(1, msg_size);
    if (!buf) return;

    s_stats = bench_stats_create(count);
    s_echo_received = 0;

    printf("Warming up (100 messages)...\n");
    for (int w = 0; w < 100; w++) {
        bench_msg_encode(buf, 0, bench_time_us());
        krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, false);
        s_wait_for_echo((LONG)(w + 1));
    }
    s_echo_received = 0;
    bench_stats_destroy(&s_stats);
    s_stats = bench_stats_create(count);

    printf("Running latency test...\n");
    uint64_t t_start = bench_time_us();

    for (uint32_t i = 0; i < count; i++) {
        bench_msg_encode(buf, (uint64_t)i, bench_time_us());
        krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, false);
        s_wait_for_echo((LONG)(i + 1));
    }

    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    LONG received = InterlockedOr(&s_echo_received, 0);
    bench_stats_set_counts(s_stats, count, (uint64_t)received,
                           (uint64_t)count * msg_size, (uint64_t)received * msg_size, duration);

    BenchResult_t result = bench_stats_compute(s_stats);
    bench_stats_print(&result, "Echo Latency (sequential)", msg_size);
    bench_stats_export_csv(s_stats, "latency_results.csv");

    bench_stats_destroy(&s_stats);
    free(buf);
}

/* ---- Test: Throughput (fire-and-forget or ACK) ---- */
static void s_run_throughput(ServerConnection_t* conn, uint32_t count,
                             uint32_t msg_size, bool require_ack) {
    uint8_t* buf = calloc(1, msg_size);
    if (!buf) return;

    s_stats = bench_stats_create(0);
    s_echo_received = 0;

    printf("Running throughput test...\n");
    uint64_t t_start = bench_time_us();

    for (uint32_t i = 0; i < count; i++) {
        bench_msg_encode(buf, (uint64_t)i, bench_time_us());
        Void_r r = krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, require_ack);
        if (!r.base.valid) {
            printf("Send failed at message %u: %s\n", i,
                   r.base.error_message ? r.base.error_message : "unknown");
            break;
        }
    }

    uint64_t t_end = bench_time_us();

    if (require_ack) Sleep(500);

    double duration = (double)(t_end - t_start) / 1000000.0;
    LONG received = InterlockedOr(&s_echo_received, 0);

    bench_stats_set_counts(s_stats, count, (uint64_t)received,
                           (uint64_t)count * msg_size, (uint64_t)received * msg_size, duration);

    BenchResult_t result = bench_stats_compute(s_stats);
    const char* name = require_ack ? "Throughput (with ACK)" : "Throughput (fire-and-forget)";
    bench_stats_print(&result, name, msg_size);

    bench_stats_destroy(&s_stats);
    free(buf);
}

/* ---- Test: Fragment Sweep (latency at different sizes) ---- */
static void s_run_fragment_sweep(ServerConnection_t* conn, uint32_t count_per_size) {
    static const uint32_t sizes[] = {64, 256, 512, 1024, 1382, 2048, 4096, 8192};
    uint32_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("Running fragment sweep (%u messages per size)...\n\n", count_per_size);

    for (uint32_t si = 0; si < num_sizes; si++) {
        uint32_t msg_size = sizes[si];
        if (msg_size < BENCH_MSG_HEADER_SIZE) msg_size = BENCH_MSG_HEADER_SIZE;

        uint8_t* buf = calloc(1, msg_size);
        if (!buf) continue;

        s_stats = bench_stats_create(count_per_size);
        s_echo_received = 0;

        uint64_t t_start = bench_time_us();

        for (uint32_t i = 0; i < count_per_size; i++) {
            bench_msg_encode(buf, (uint64_t)i, bench_time_us());
            krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, false);
            s_wait_for_echo((LONG)(i + 1));
        }

        uint64_t t_end = bench_time_us();
        double duration = (double)(t_end - t_start) / 1000000.0;

        LONG received = InterlockedOr(&s_echo_received, 0);
        bench_stats_set_counts(s_stats, count_per_size, (uint64_t)received,
                               (uint64_t)count_per_size * msg_size,
                               (uint64_t)received * msg_size, duration);

        BenchResult_t result = bench_stats_compute(s_stats);

        bool fragmented = (msg_size > 1382);
        char name[64];
        snprintf(name, sizeof(name), "Size %u bytes%s", msg_size,
                 fragmented ? " (fragmented)" : "");
        bench_stats_print(&result, name, msg_size);

        char csv_name[64];
        snprintf(csv_name, sizeof(csv_name), "fragment_%u.csv", msg_size);
        bench_stats_export_csv(s_stats, csv_name);

        bench_stats_destroy(&s_stats);
        free(buf);
    }
}

/* ---- Test: Connection Setup Latency ---- */
static void s_run_connect(const PortAddress_t* server_addr, uint32_t count) {
    s_stats = bench_stats_create(count);

    printf("Running connection test (%u cycles)...\n", count);
    uint64_t t_start = bench_time_us();

    uint32_t success = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t c_start = bench_time_us();
        ServerConnection_t* conn = krs_client_server_connect(*server_addr);
        uint64_t c_end = bench_time_us();

        if (!conn) {
            printf("  Connect %u/%u failed\n", i + 1, count);
            continue;
        }

        bench_stats_record_latency(s_stats, (double)(c_end - c_start));
        krs_client_disconnect(&conn);
        success++;

        Sleep(10);
    }

    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    bench_stats_set_counts(s_stats, count, success, 0, 0, duration);
    BenchResult_t result = bench_stats_compute(s_stats);
    bench_stats_print(&result, "Connection Setup", 0);
    bench_stats_export_csv(s_stats, "connect_results.csv");

    bench_stats_destroy(&s_stats);
}

/* ---- CLI ---- */
static void s_print_usage(void) {
    printf("Usage: bench_client [options]\n");
    printf("  --host HOST    Server IP (default: 127.0.0.1)\n");
    printf("  --port PORT    Server port (default: 9001)\n");
    printf("  --test TEST    latency        Sequential echo RTT\n");
    printf("                 throughput     Fire-and-forget send rate\n");
    printf("                 throughput-ack Send rate with ACK tracking\n");
    printf("                 fragment       Size sweep (64B - 8KB)\n");
    printf("                 connect        Handshake latency\n");
    printf("  --count N      Messages per test (default: 10000)\n");
    printf("  --size N       Message size in bytes (default: 64, min: 16)\n");
    printf("  -h, --help     Show this help\n");
    printf("\nRecommended server modes:\n");
    printf("  latency/fragment/connect  -> bench_server --mode echo\n");
    printf("  throughput/throughput-ack  -> bench_server --mode sink\n");
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    uint16_t port = 9001;
    TestMode_e test = TEST_LATENCY;
    uint32_t count = 10000;
    uint32_t msg_size = 64;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "latency") == 0)          test = TEST_LATENCY;
            else if (strcmp(argv[i], "throughput") == 0)    test = TEST_THROUGHPUT;
            else if (strcmp(argv[i], "throughput-ack") == 0) test = TEST_THROUGHPUT_ACK;
            else if (strcmp(argv[i], "fragment") == 0)      test = TEST_FRAGMENT;
            else if (strcmp(argv[i], "connect") == 0)       test = TEST_CONNECT;
            else { printf("Unknown test: %s\n", argv[i]); s_print_usage(); return 1; }
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            msg_size = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            s_print_usage();
            return 0;
        }
    }

    if (msg_size < BENCH_MSG_HEADER_SIZE) msg_size = BENCH_MSG_HEADER_SIZE;

    bench_timer_init();

    const char* test_names[] = {"latency", "throughput", "throughput-ack", "fragment", "connect"};
    printf("=== Kronos Benchmark Client ===\n");
    printf("Server: %s:%u  |  Test: %s  |  Count: %u  |  Size: %u\n\n",
           host, port, test_names[test], count, msg_size);

    Address_t addr = krs_network_address_ipv4_create(host);
    PortAddress_t server_addr = krs_network_port_address_create(port, addr);

    /* Connect test has its own connect/disconnect cycle */
    if (test == TEST_CONNECT) {
        s_run_connect(&server_addr, count);
        printf("Done.\n");
        return 0;
    }

    /* All other tests: connect once, run test, disconnect */
    ServerConnection_t* conn = krs_client_server_connect(server_addr);
    if (!conn) {
        printf("Connection failed. Is the server running?\n");
        return 1;
    }
    printf("Connected (id=%u)\n\n", conn->connection_id);

    Void_r sub_r = krs_client_subscribe(conn, BENCH_CHANNEL, 2000);
    if (!sub_r.base.valid) {
        printf("Subscribe failed: %s\n",
               sub_r.base.error_message ? sub_r.base.error_message : "unknown");
        krs_client_disconnect(&conn);
        return 1;
    }

    krs_client_set_callback(conn, s_on_echo, NULL);
    krs_client_start_receive(conn);

    Sleep(100); /* Let heartbeat/recv thread stabilize */

    switch (test) {
        case TEST_LATENCY:
            s_run_latency(conn, count, msg_size);
            break;
        case TEST_THROUGHPUT:
            s_run_throughput(conn, count, msg_size, false);
            break;
        case TEST_THROUGHPUT_ACK:
            s_run_throughput(conn, count, msg_size, true);
            break;
        case TEST_FRAGMENT:
            s_run_fragment_sweep(conn, count);
            break;
        default:
            break;
    }

    krs_client_disconnect(&conn);
    printf("Done.\n");
    return 0;
}
