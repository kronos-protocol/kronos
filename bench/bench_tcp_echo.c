#include <winsock2.h>
#include <ws2tcpip.h>

#include "bench_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

typedef enum { ROLE_SERVER, ROLE_CLIENT } Role_e;

static volatile bool s_running = true;

static BOOL WINAPI s_console_handler(DWORD ctrl) {
    (void)ctrl;
    s_running = false;
    return TRUE;
}

static bool s_recv_full(SOCKET sock, uint8_t* buf, uint32_t need) {
    uint32_t got = 0;
    while (got < need) {
        int n = recv(sock, (char*)buf + got, (int)(need - got), 0);
        if (n <= 0) return false;
        got += (uint32_t)n;
    }
    return true;
}

static bool s_send_full(SOCKET sock, const uint8_t* buf, uint32_t need) {
    uint32_t sent = 0;
    while (sent < need) {
        int n = send(sock, (const char*)buf + sent, (int)(need - sent), 0);
        if (n <= 0) return false;
        sent += (uint32_t)n;
    }
    return true;
}

static int s_run_server(uint16_t port) {
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        return 1;
    }

    BOOL on = TRUE;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&on, sizeof(on));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed on TCP port %u: %d\n", port, WSAGetLastError());
        closesocket(listen_sock);
        return 1;
    }
    if (listen(listen_sock, 4) == SOCKET_ERROR) {
        fprintf(stderr, "listen() failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        return 1;
    }

    printf("[tcp-echo server] Listening on TCP port %u\n", port);
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (s_running) {
        struct sockaddr_in caddr;
        int caddr_len = sizeof(caddr);
        SOCKET cs = accept(listen_sock, (struct sockaddr*)&caddr, &caddr_len);
        if (cs == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) break;
            continue;
        }

        BOOL nodelay = TRUE;
        setsockopt(cs, IPPROTO_TCP, TCP_NODELAY,
                   (const char*)&nodelay, sizeof(nodelay));

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, sizeof(ipbuf));
        printf("[tcp-echo server] client %s:%u connected\n",
               ipbuf, ntohs(caddr.sin_port));
        fflush(stdout);

        uint8_t buf[65536];
        while (s_running) {
            int n = recv(cs, (char*)buf, sizeof(buf), 0);
            if (n <= 0) break;
            if (!s_send_full(cs, buf, (uint32_t)n)) break;
        }

        closesocket(cs);
        printf("[tcp-echo server] client disconnected\n");
        fflush(stdout);
    }

    closesocket(listen_sock);
    return 0;
}

static int s_run_client(const char* host, uint16_t port, uint32_t count,
                        uint32_t msg_size, uint32_t warmup) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &dst.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed for %s\n", host);
        closesocket(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&dst, sizeof(dst)) == SOCKET_ERROR) {
        fprintf(stderr, "connect() failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return 1;
    }

    /* TCP_NODELAY: disable Nagle's algorithm. Without this, small RPC-style
     * messages get coalesced behind the 200 ms Nagle timer and per-message RTT
     * is dominated by that delay rather than by the kernel/loopback round-trip.
     * The whole point of this baseline is to measure fast loopback RTT, so
     * Nagle has to go. */
    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
               (const char*)&nodelay, sizeof(nodelay));

    if (msg_size < BENCH_MSG_HEADER_SIZE) msg_size = BENCH_MSG_HEADER_SIZE;

    uint8_t* buf = calloc(1, msg_size);
    if (!buf) { closesocket(sock); return 1; }

    BenchStats_t* stats = bench_stats_create(count);
    if (!stats) { free(buf); closesocket(sock); return 1; }

    printf("[tcp-echo client] Target %s:%u  size=%u  warmup=%u  count=%u\n",
           host, port, msg_size, warmup, count);
    fflush(stdout);

    for (uint32_t w = 0; w < warmup; w++) {
        bench_msg_encode(buf, (uint64_t)w, bench_time_us());
        if (!s_send_full(sock, buf, msg_size) ||
            !s_recv_full(sock, buf, msg_size)) {
            fprintf(stderr, "[warmup] socket failure: %d\n", WSAGetLastError());
            free(buf); bench_stats_destroy(&stats); closesocket(sock); return 1;
        }
    }

    uint64_t t_start = bench_time_us();
    uint32_t recv_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t t_send = bench_time_us();
        bench_msg_encode(buf, (uint64_t)i, t_send);
        if (!s_send_full(sock, buf, msg_size)) break;
        if (!s_recv_full(sock, buf, msg_size)) break;
        uint64_t t_recv = bench_time_us();
        bench_stats_record_latency(stats, (double)(t_recv - t_send));
        recv_count++;
    }
    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    bench_stats_set_counts(stats, count, recv_count,
                           (uint64_t)count * msg_size,
                           (uint64_t)recv_count * msg_size,
                           duration);

    BenchResult_t result = bench_stats_compute(stats);
    bench_stats_print(&result, "TCP Echo Loopback - AGGREGATE", msg_size);

    free(buf);
    bench_stats_destroy(&stats);
    closesocket(sock);
    return 0;
}

static void s_print_usage(void) {
    printf("Usage: bench_tcp_echo --mode {server|client} [options]\n");
    printf("  --mode server    Run echo server on --port (default 9001)\n");
    printf("  --mode client    Run echo client against --host:--port\n");
    printf("Common options:\n");
    printf("  --host HOST      Server IP (client only, default 127.0.0.1)\n");
    printf("  --port PORT      TCP port (default 9001)\n");
    printf("  --count N        Measurement iterations (client, default 5000)\n");
    printf("  --warmup N       Warm-up iterations (client, default 500)\n");
    printf("  --size N         Message size in bytes (client, default 64, min 16)\n");
    printf("  -h, --help       Show this help\n");
}

int main(int argc, char* argv[]) {
    Role_e role = ROLE_CLIENT;
    bool role_set = false;
    const char* host = "127.0.0.1";
    uint16_t port = 9001;
    uint32_t count = 5000;
    uint32_t warmup = 500;
    uint32_t size = 64;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "server") == 0) {
                role = ROLE_SERVER;
                role_set = true;
            } else if (strcmp(argv[i], "client") == 0) {
                role = ROLE_CLIENT;
                role_set = true;
            } else {
                s_print_usage();
                return 1;
            }
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            warmup = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            size = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            s_print_usage();
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            s_print_usage();
            return 1;
        }
    }

    if (!role_set) {
        s_print_usage();
        return 1;
    }

    bench_timer_init();
    SetConsoleCtrlHandler(s_console_handler, TRUE);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    int rc = (role == ROLE_SERVER)
                 ? s_run_server(port)
                 : s_run_client(host, port, count, size, warmup);

    WSACleanup();
    return rc;
}
