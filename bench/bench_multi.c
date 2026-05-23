#include "kronos_client.h"
#include "kronos_network.h"
#include "kronos_error.h"
#include "client_internal.h"
#include "bench_common.h"
#include "network_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef enum {
    MULTI_LATENCY,
    MULTI_LATENCY_ACK,
    MULTI_LATENCY_ACK_PIPELINED,
    MULTI_THROUGHPUT,
    MULTI_THROUGHPUT_ACK,
    MULTI_FRAGMENT,
    MULTI_CONNECT,
    MULTI_ALL
} MultiTestMode_e;

typedef struct {
    uint32_t       client_id;
    PortAddress_t  server_addr;
    MultiTestMode_e test;
    uint32_t       count;
    uint32_t       msg_size;
    uint32_t       rate;
    bool           fast_retransmit_enabled;
    BenchResult_t  result;
    bool           success;
} ClientThreadArgs_t;

/* ---- Per-client echo tracking ---- */
/* Each client thread uses thread-local stats and echo counter.
   We use the args struct to hold the per-client state. */

typedef struct {
    BenchStats_t*  stats;
    volatile LONG  echo_received;
} ClientState_t;

static __declspec(thread) ClientState_t* tls_state = NULL;

static void s_on_echo(Channel_t channel, uint32_t connection_id,
                      const uint8_t* data, uint16_t data_length,
                      void* user_data) {
    (void)channel; (void)connection_id;
    ClientState_t* state = (ClientState_t*)user_data;
    if (!state) return;

    if (data_length >= BENCH_MSG_HEADER_SIZE && state->stats) {
        uint64_t seq, original_ts;
        bench_msg_decode(data, &seq, &original_ts);
        double rtt = (double)(bench_time_us() - original_ts);
        bench_stats_record_latency(state->stats, rtt);
    }
    InterlockedIncrement(&state->echo_received);
}

static inline void s_wait_echo(ClientState_t* state, LONG expected) {
    while (InterlockedOr(&state->echo_received, 0) < expected) {
        Sleep(0);
    }
}

/* ---- Individual test implementations (per-client) ---- */

static BenchResult_t s_client_latency(ServerConnection_t* conn, ClientState_t* state,
                                       uint32_t count, uint32_t msg_size) {
    uint8_t* buf = calloc(1, msg_size);
    BenchResult_t empty = {0};
    if (!buf) return empty;

    state->stats = bench_stats_create(count);
    state->echo_received = 0;

    /* warmup */
    for (int w = 0; w < 50; w++) {
        bench_msg_encode(buf, 0, bench_time_us());
        krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, false);
        s_wait_echo(state, (LONG)(w + 1));
    }
    state->echo_received = 0;
    bench_stats_destroy(&state->stats);
    state->stats = bench_stats_create(count);

    uint64_t t_start = bench_time_us();
    for (uint32_t i = 0; i < count; i++) {
        bench_msg_encode(buf, (uint64_t)i, bench_time_us());
        krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, false);
        s_wait_echo(state, (LONG)(i + 1));
    }
    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    LONG received = InterlockedOr(&state->echo_received, 0);
    bench_stats_set_counts(state->stats, count, (uint64_t)received,
                           (uint64_t)count * msg_size, (uint64_t)received * msg_size, duration);

    BenchResult_t result = bench_stats_compute(state->stats);
    bench_stats_destroy(&state->stats);
    free(buf);
    return result;
}

static BenchResult_t s_client_latency_ack(ServerConnection_t* conn, ClientState_t* state,
                                           uint32_t count, uint32_t msg_size, uint32_t rate) {
    uint8_t* buf = calloc(1, msg_size);
    BenchResult_t empty = {0};
    if (!buf) return empty;

    state->stats = bench_stats_create(count);
    state->echo_received = 0;

    RateLimiter_t rl = bench_rate_limiter_create(rate);

    for (int w = 0; w < 20; w++) {
        bench_msg_encode(buf, 0, bench_time_us());
        uint32_t retries = 0;
        while (retries < 200) {
            Void_r r = krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, true);
            if (r.base.valid) break;
            if (r.base.error_code == KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) {
                Sleep(1);
                retries++;
            } else {
                break;
            }
        }
        s_wait_echo(state, (LONG)(w + 1));
    }
    state->echo_received = 0;
    bench_stats_destroy(&state->stats);
    state->stats = bench_stats_create(count);

    uint32_t actual_sent = 0;
    uint64_t t_start = bench_time_us();
    for (uint32_t i = 0; i < count; i++) {
        bench_rate_limiter_wait(&rl);
        bench_msg_encode(buf, (uint64_t)i, bench_time_us());

        uint32_t retries = 0;
        while (retries < 500) {
            Void_r r = krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, true);
            if (r.base.valid) {
                actual_sent++;
                break;
            }
            if (r.base.error_code == KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) {
                Sleep(1);
                retries++;
            } else {
                goto latency_ack_done;
            }
        }
        if (retries >= 500) goto latency_ack_done;
        s_wait_echo(state, (LONG)(actual_sent));
    }
    latency_ack_done:;
    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    LONG received = InterlockedOr(&state->echo_received, 0);
    bench_stats_set_counts(state->stats, actual_sent, (uint64_t)received,
                           (uint64_t)actual_sent * msg_size, (uint64_t)received * msg_size, duration);

    BenchResult_t result = bench_stats_compute(state->stats);
    bench_stats_destroy(&state->stats);
    free(buf);
    return result;
}

static BenchResult_t s_client_latency_ack_pipelined(ServerConnection_t* conn, ClientState_t* state,
                                                     uint32_t count, uint32_t msg_size, uint32_t rate) {
    uint8_t* buf = calloc(1, msg_size);
    BenchResult_t empty = {0};
    if (!buf) return empty;

    state->stats = bench_stats_create(count);
    state->echo_received = 0;

    RateLimiter_t rl = bench_rate_limiter_create(rate);

    for (int w = 0; w < 20; w++) {
        bench_msg_encode(buf, 0, bench_time_us());
        uint32_t retries = 0;
        while (retries < 200) {
            Void_r r = krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, true);
            if (r.base.valid) break;
            if (r.base.error_code == KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) {
                Sleep(1); retries++;
            } else { break; }
        }
    }
    uint64_t warm_deadline = bench_time_us() + 2000000;
    while (bench_time_us() < warm_deadline) {
        if ((uint32_t)InterlockedOr(&state->echo_received, 0) >= 20) break;
        Sleep(5);
    }
    state->echo_received = 0;
    bench_stats_destroy(&state->stats);
    state->stats = bench_stats_create(count);

    uint32_t actual_sent = 0;
    uint64_t t_start = bench_time_us();
    for (uint32_t i = 0; i < count; i++) {
        bench_rate_limiter_wait(&rl);
        bench_msg_encode(buf, (uint64_t)i, bench_time_us());
        uint32_t retries = 0;
        while (retries < 30000) {
            Void_r r = krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, true);
            if (r.base.valid) {
                actual_sent++;
                break;
            }
            if (r.base.error_code == KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) {
                Sleep(1); retries++;
            } else { goto pipe_done; }
        }
        if (retries >= 30000) goto pipe_done;
    }
pipe_done:;

    uint64_t deadline = bench_time_us() + 60000000;
    while (bench_time_us() < deadline) {
        if ((uint32_t)InterlockedOr(&state->echo_received, 0) >= actual_sent) break;
        Sleep(10);
    }
    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    LONG received = InterlockedOr(&state->echo_received, 0);
    bench_stats_set_counts(state->stats, actual_sent, (uint64_t)received,
                           (uint64_t)actual_sent * msg_size, (uint64_t)received * msg_size, duration);

    BenchResult_t result = bench_stats_compute(state->stats);
    bench_stats_destroy(&state->stats);
    free(buf);
    return result;
}

static BenchResult_t s_client_throughput(ServerConnection_t* conn, ClientState_t* state,
                                          uint32_t count, uint32_t msg_size, bool require_ack,
                                          uint32_t rate) {
    uint8_t* buf = calloc(1, msg_size);
    BenchResult_t empty = {0};
    if (!buf) return empty;

    state->stats = bench_stats_create(0);
    state->echo_received = 0;

    RateLimiter_t rl = bench_rate_limiter_create(rate);
    uint32_t actual_sent = 0;
    uint32_t cc_waits = 0;

    uint64_t t_start = bench_time_us();
    for (uint32_t i = 0; i < count; i++) {
        bench_rate_limiter_wait(&rl);
        bench_msg_encode(buf, (uint64_t)i, bench_time_us());

        uint32_t retries = 0;
        while (retries < 500) {
            Void_r r = krs_client_send(conn, BENCH_CHANNEL, buf, (uint16_t)msg_size, require_ack);
            if (r.base.valid) {
                actual_sent++;
                break;
            }
            if (r.base.error_code == KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) {
                Sleep(1);
                retries++;
                cc_waits++;
            } else {
                goto send_done;
            }
        }
        if (retries >= 500) goto send_done;
    }
    send_done:;
    uint64_t t_end = bench_time_us();

    if (require_ack) {
        uint64_t wait_start = bench_time_us();
        while (bench_time_us() - wait_start < 2000000) {
            LONG rx = InterlockedOr(&state->echo_received, 0);
            if ((uint32_t)rx >= actual_sent) break;
            Sleep(10);
        }
    }

    double duration = (double)(t_end - t_start) / 1000000.0;
    LONG received = InterlockedOr(&state->echo_received, 0);

    bench_stats_set_counts(state->stats, actual_sent, (uint64_t)received,
                           (uint64_t)actual_sent * msg_size, (uint64_t)received * msg_size, duration);

    BenchResult_t result = bench_stats_compute(state->stats);
    bench_stats_destroy(&state->stats);
    free(buf);
    return result;
}

static BenchResult_t s_client_connect(const PortAddress_t* server_addr, ClientState_t* state,
                                       uint32_t count) {
    state->stats = bench_stats_create(count);

    uint64_t t_start = bench_time_us();
    uint32_t success = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t c_start = bench_time_us();
        ServerConnection_t* conn = krs_client_server_connect(*server_addr);
        uint64_t c_end = bench_time_us();
        if (!conn) continue;
        bench_stats_record_latency(state->stats, (double)(c_end - c_start));
        krs_client_disconnect(&conn);
        success++;
        Sleep(10);
    }
    uint64_t t_end = bench_time_us();

    bench_stats_set_counts(state->stats, count, success, 0, 0,
                           (double)(t_end - t_start) / 1000000.0);
    BenchResult_t result = bench_stats_compute(state->stats);
    bench_stats_destroy(&state->stats);
    return result;
}

/* ---- Client thread entry ---- */

static DWORD WINAPI s_client_thread(LPVOID param) {
    ClientThreadArgs_t* args = (ClientThreadArgs_t*)param;
    bench_timer_init();

    ClientState_t state = {0};

    if (args->test == MULTI_CONNECT) {
        args->result = s_client_connect(&args->server_addr, &state, args->count);
        args->success = true;
        return 0;
    }

    ServerConnection_t* conn = krs_client_server_connect(args->server_addr);
    if (!conn) {
        printf("[Client %u] Connection failed\n", args->client_id);
        args->success = false;
        return 1;
    }

    Void_r sub_r = krs_client_subscribe(conn, BENCH_CHANNEL, 2000);
    if (!sub_r.base.valid) {
        printf("[Client %u] Subscribe failed\n", args->client_id);
        args->success = false;
        krs_client_disconnect(&conn);
        return 1;
    }

    krs_client_set_callback(conn, s_on_echo, &state);
    krs_client_start_receive(conn);
    Sleep(50);

    /* Force AckTracker creation BEFORE applying the FR-disable. The tracker is
     * lazily created inside the first require_ack send, so an early call to
     * krs_ack_tracker_set_fast_retransmit_enabled(conn->ack_tracker, false)
     * would silently no-op against a NULL pointer and FR would stay enabled in
     * the supposed "FR-off" half of the A/B. Each test function resets
     * state->echo_received and rebuilds stats during its own warmup, so this
     * priming send does not pollute measurements. */
    {
        uint8_t prime[1] = {0};
        krs_client_send(conn, BENCH_CHANNEL, prime, 1, true);
        Sleep(5);
    }
    if (!args->fast_retransmit_enabled && conn->ack_tracker) {
        krs_ack_tracker_set_fast_retransmit_enabled(conn->ack_tracker, false);
    }

    switch (args->test) {
        case MULTI_LATENCY:
            args->result = s_client_latency(conn, &state, args->count, args->msg_size);
            break;
        case MULTI_LATENCY_ACK:
            args->result = s_client_latency_ack(conn, &state, args->count, args->msg_size, args->rate);
            break;
        case MULTI_LATENCY_ACK_PIPELINED:
            args->result = s_client_latency_ack_pipelined(conn, &state, args->count, args->msg_size, args->rate);
            break;
        case MULTI_THROUGHPUT:
            args->result = s_client_throughput(conn, &state, args->count, args->msg_size, false, args->rate);
            break;
        case MULTI_THROUGHPUT_ACK:
            args->result = s_client_throughput(conn, &state, args->count, args->msg_size, true, args->rate);
            break;
        default:
            break;
    }

    args->success = true;
    krs_client_disconnect(&conn);
    return 0;
}

/* ---- Run a single test with N clients ---- */

static void s_run_test(MultiTestMode_e test, const PortAddress_t* server_addr,
                       uint32_t num_clients, uint32_t count, uint32_t msg_size,
                       uint32_t rate, bool fast_retransmit_enabled) {
    const char* test_names[] = {"Latency", "Latency (ACK)", "Latency (ACK pipelined)",
                                "Throughput", "Throughput (ACK)", "Fragment", "Connect"};
    const char* test_name = test_names[test];

    printf("\n--- %s with %u client(s) ---\n", test_name, num_clients);

    ClientThreadArgs_t* args = calloc(num_clients, sizeof(ClientThreadArgs_t));
    HANDLE* threads = calloc(num_clients, sizeof(HANDLE));
    if (!args || !threads) {
        printf("Allocation failed\n");
        free(args); free(threads);
        return;
    }

    for (uint32_t i = 0; i < num_clients; i++) {
        args[i].client_id = i + 1;
        args[i].server_addr = *server_addr;
        args[i].test = test;
        args[i].count = count;
        args[i].msg_size = msg_size;
        args[i].rate = rate;
        args[i].fast_retransmit_enabled = fast_retransmit_enabled;
    }

    uint64_t t_start = bench_time_us();

    for (uint32_t i = 0; i < num_clients; i++) {
        threads[i] = CreateThread(NULL, 0, s_client_thread, &args[i], 0, NULL);
        if (!threads[i]) {
            printf("[Client %u] Thread creation failed\n", i + 1);
        }
        Sleep(50);
    }

    WaitForMultipleObjects(num_clients, threads, TRUE, 300000);

    uint64_t t_end = bench_time_us();
    double wall_time = (double)(t_end - t_start) / 1000000.0;

    for (uint32_t i = 0; i < num_clients; i++) {
        if (threads[i]) CloseHandle(threads[i]);
    }

    /* Print per-client results */
    uint32_t success_count = 0;
    BenchResult_t* results = calloc(num_clients, sizeof(BenchResult_t));

    for (uint32_t i = 0; i < num_clients; i++) {
        if (!args[i].success) {
            printf("[Client %u] FAILED\n", args[i].client_id);
            continue;
        }
        results[success_count] = args[i].result;
        success_count++;

        if (num_clients <= 10) {
            char label[64];
            snprintf(label, sizeof(label), "%s - Client %u", test_name, args[i].client_id);
            bench_stats_print(&args[i].result, label, msg_size);
        }
    }

    /* Aggregate */
    if (success_count > 0) {
        BenchResult_t merged = bench_stats_merge_results(results, success_count);
        merged.duration_s = wall_time;
        if (wall_time > 0.0) {
            merged.send_rate_msg_s = (double)merged.msg_sent / wall_time;
            merged.recv_rate_msg_s = (double)merged.msg_received / wall_time;
            merged.send_bw_mbps = (double)merged.bytes_sent / wall_time / (1024.0 * 1024.0);
            merged.recv_bw_mbps = (double)merged.bytes_received / wall_time / (1024.0 * 1024.0);
        }

        char agg_label[64];
        snprintf(agg_label, sizeof(agg_label), "%s - AGGREGATE (%u clients)", test_name, success_count);
        bench_stats_print(&merged, agg_label, msg_size);
    }

    free(results);
    free(args);
    free(threads);

    Sleep(2000);
}

/* ---- Fragment sweep (always single client, sequential sizes) ---- */

static void s_run_fragment_sweep(const PortAddress_t* server_addr, uint32_t num_clients,
                                  uint32_t count_per_size, uint32_t rate,
                                  bool fast_retransmit_enabled) {
    static const uint32_t sizes[] = {64, 256, 512, 1024, 1382, 2048, 4096, 8192};
    uint32_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("\n=== Fragment Sweep (%u client(s), %u messages per size) ===\n",
           num_clients, count_per_size);

    for (uint32_t si = 0; si < num_sizes; si++) {
        uint32_t sz = sizes[si];
        if (sz < BENCH_MSG_HEADER_SIZE) sz = BENCH_MSG_HEADER_SIZE;
        s_run_test(MULTI_LATENCY, server_addr, num_clients, count_per_size, sz, rate,
                   fast_retransmit_enabled);
    }
}

/* ---- CLI ---- */

static void s_print_usage(void) {
    printf("Usage: bench_multi [options]\n");
    printf("  --host HOST      Server IP (default: 127.0.0.1)\n");
    printf("  --port PORT      Server port (default: 9001)\n");
    printf("  --clients N      Number of concurrent clients (default: 4)\n");
    printf("  --test TEST      latency, latency-ack, latency-ack-pipelined,\n");
    printf("                   throughput, throughput-ack, fragment, connect, all\n");
    printf("                   (default: all)\n");
    printf("  --count N        Messages per client per test (default: 5000)\n");
    printf("  --size N         Message size in bytes (default: 64, min: 16)\n");
    printf("  --rate N         Send rate per client in msg/s (default: 0 = unlimited)\n");
    printf("  --no-fast-retransmit  Disable fast retransmit on each client's AckTracker\n");
    printf("  -h, --help       Show this help\n");
    printf("\nServer should be running: bench_server --mode echo\n");
    printf("(For throughput-only tests, use --mode sink)\n");
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    uint16_t port = 9001;
    uint32_t num_clients = 4;
    MultiTestMode_e test = MULTI_ALL;
    uint32_t count = 5000;
    uint32_t msg_size = 64;
    uint32_t rate = 0;
    bool fast_retransmit_enabled = true;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--clients") == 0 && i + 1 < argc) {
            num_clients = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "latency") == 0)           test = MULTI_LATENCY;
            else if (strcmp(argv[i], "latency-ack") == 0)   test = MULTI_LATENCY_ACK;
            else if (strcmp(argv[i], "latency-ack-pipelined") == 0) test = MULTI_LATENCY_ACK_PIPELINED;
            else if (strcmp(argv[i], "throughput") == 0)     test = MULTI_THROUGHPUT;
            else if (strcmp(argv[i], "throughput-ack") == 0) test = MULTI_THROUGHPUT_ACK;
            else if (strcmp(argv[i], "fragment") == 0)       test = MULTI_FRAGMENT;
            else if (strcmp(argv[i], "connect") == 0)        test = MULTI_CONNECT;
            else if (strcmp(argv[i], "all") == 0)            test = MULTI_ALL;
            else { printf("Unknown test: %s\n", argv[i]); s_print_usage(); return 1; }
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            msg_size = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            rate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-fast-retransmit") == 0) {
            fast_retransmit_enabled = false;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            s_print_usage(); return 0;
        }
    }

    if (msg_size < BENCH_MSG_HEADER_SIZE) msg_size = BENCH_MSG_HEADER_SIZE;

    bench_timer_init();
    krs_wsa_init();

    printf("=== Kronos Multi-Client Benchmark ===\n");
    if (rate > 0) {
        printf("Server: %s:%u  |  Clients: %u  |  Count: %u  |  Size: %u  |  Rate: %u msg/s\n\n",
               host, port, num_clients, count, msg_size, rate);
    } else {
        printf("Server: %s:%u  |  Clients: %u  |  Count: %u  |  Size: %u  |  Rate: unlimited\n\n",
               host, port, num_clients, count, msg_size);
    }

    Address_t addr = krs_network_address_ipv4_create(host);
    PortAddress_t server_addr = krs_network_port_address_create(port, addr);

    if (test == MULTI_ALL) {
        printf("Running all tests...\n");
        s_run_test(MULTI_CONNECT, &server_addr, num_clients, count / 10, msg_size, 0,
                   fast_retransmit_enabled);
        s_run_test(MULTI_LATENCY, &server_addr, num_clients, count, msg_size, 0,
                   fast_retransmit_enabled);
        s_run_test(MULTI_LATENCY_ACK, &server_addr, num_clients, count / 2, msg_size, rate,
                   fast_retransmit_enabled);
        s_run_test(MULTI_THROUGHPUT, &server_addr, num_clients, count, msg_size, 0,
                   fast_retransmit_enabled);
        printf("\nDraining server queue before ACK tests...\n");
        Sleep(3000);
        s_run_test(MULTI_THROUGHPUT_ACK, &server_addr, num_clients, count / 2, msg_size, rate,
                   fast_retransmit_enabled);
        s_run_fragment_sweep(&server_addr, num_clients, count / 5, rate,
                             fast_retransmit_enabled);
    } else if (test == MULTI_FRAGMENT) {
        s_run_fragment_sweep(&server_addr, num_clients, count, rate,
                             fast_retransmit_enabled);
    } else {
        s_run_test(test, &server_addr, num_clients, count, msg_size, rate,
                   fast_retransmit_enabled);
    }

    printf("\n=== All tests complete ===\n");
    krs_wsa_cleanup();
    return 0;
}
