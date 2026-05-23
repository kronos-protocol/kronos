#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")


#define KRS_PROXY_BUFFER_SIZE 2048
#define KRS_PROXY_DEFAULT_LISTEN_PORT 9100
#define KRS_PROXY_DEFAULT_TARGET_HOST "127.0.0.1"
#define KRS_PROXY_DEFAULT_TARGET_PORT 9001
#define KRS_PROXY_DEFAULT_SEED 42u
#define KRS_PROXY_DEFAULT_STATS_INTERVAL_S 1

typedef struct {
    uint16_t    listen_port;
    const char* target_host;
    uint16_t    target_port;
    double      loss_up;
    double      loss_down;
    uint32_t    delay_ms;
    uint32_t    seed;
    uint32_t    stats_interval_s;
} ProxyConfig_t;

typedef struct {
    volatile LONG64 forwarded_up;
    volatile LONG64 dropped_up;
    volatile LONG64 forwarded_down;
    volatile LONG64 dropped_down;
} ProxyStats_t;

static volatile bool s_running = true;
static ProxyStats_t  s_stats = {0};

static uint32_t s_rng_state = KRS_PROXY_DEFAULT_SEED;

static uint32_t s_rng_next(void) {
    s_rng_state = s_rng_state * 1103515245u + 12345u;
    return (s_rng_state >> 16) & 0x7FFFu;
}

static double s_rng_uniform(void) {
    return (double)s_rng_next() / 32768.0;
}

static BOOL WINAPI s_console_handler(DWORD ctrl) {
    (void)ctrl;
    s_running = false;
    return TRUE;
}

static void s_print_usage(const char* argv0) {
    printf("Usage: %s [options]\n", argv0);
    printf("  --listen-port PORT       Port the client connects to (default: %d)\n", KRS_PROXY_DEFAULT_LISTEN_PORT);
    printf("  --target-host HOST       Server host (default: %s)\n", KRS_PROXY_DEFAULT_TARGET_HOST);
    printf("  --target-port PORT       Server port (default: %d)\n", KRS_PROXY_DEFAULT_TARGET_PORT);
    printf("  --loss-up PCT            Drop rate client->server, 0.0-1.0 (default: 0.0)\n");
    printf("  --loss-down PCT          Drop rate server->client, 0.0-1.0 (default: 0.0)\n");
    printf("  --delay-ms N             Fixed delay applied each direction (default: 0)\n");
    printf("  --seed N                 PRNG seed (default: %u)\n", KRS_PROXY_DEFAULT_SEED);
    printf("  --stats-interval-s N     Stats print interval, 0=disable (default: %d)\n",
           KRS_PROXY_DEFAULT_STATS_INTERVAL_S);
    printf("  -h, --help               Show this help\n");
}

static bool s_parse_double(const char* s, double* out) {
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return false;
    if (v < 0.0 || v > 1.0) return false;
    *out = v;
    return true;
}

static bool s_parse_uint(const char* s, uint32_t* out) {
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s || *end != '\0') return false;
    *out = (uint32_t)v;
    return true;
}

static bool s_parse_args(int argc, char* argv[], ProxyConfig_t* cfg) {
    cfg->listen_port      = KRS_PROXY_DEFAULT_LISTEN_PORT;
    cfg->target_host      = KRS_PROXY_DEFAULT_TARGET_HOST;
    cfg->target_port      = KRS_PROXY_DEFAULT_TARGET_PORT;
    cfg->loss_up          = 0.0;
    cfg->loss_down        = 0.0;
    cfg->delay_ms         = 0;
    cfg->seed             = KRS_PROXY_DEFAULT_SEED;
    cfg->stats_interval_s = KRS_PROXY_DEFAULT_STATS_INTERVAL_S;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            s_print_usage(argv[0]);
            exit(0);
        } else if (strcmp(a, "--listen-port") == 0 && i + 1 < argc) {
            uint32_t v; if (!s_parse_uint(argv[++i], &v) || v > 65535) return false;
            cfg->listen_port = (uint16_t)v;
        } else if (strcmp(a, "--target-host") == 0 && i + 1 < argc) {
            cfg->target_host = argv[++i];
        } else if (strcmp(a, "--target-port") == 0 && i + 1 < argc) {
            uint32_t v; if (!s_parse_uint(argv[++i], &v) || v > 65535) return false;
            cfg->target_port = (uint16_t)v;
        } else if (strcmp(a, "--loss-up") == 0 && i + 1 < argc) {
            if (!s_parse_double(argv[++i], &cfg->loss_up)) return false;
        } else if (strcmp(a, "--loss-down") == 0 && i + 1 < argc) {
            if (!s_parse_double(argv[++i], &cfg->loss_down)) return false;
        } else if (strcmp(a, "--delay-ms") == 0 && i + 1 < argc) {
            if (!s_parse_uint(argv[++i], &cfg->delay_ms)) return false;
        } else if (strcmp(a, "--seed") == 0 && i + 1 < argc) {
            if (!s_parse_uint(argv[++i], &cfg->seed)) return false;
        } else if (strcmp(a, "--stats-interval-s") == 0 && i + 1 < argc) {
            if (!s_parse_uint(argv[++i], &cfg->stats_interval_s)) return false;
        } else {
            fprintf(stderr, "Unknown or malformed option: %s\n", a);
            return false;
        }
    }
    return true;
}

static void s_print_stats(const ProxyConfig_t* cfg) {
    LONG64 fu = InterlockedOr64(&s_stats.forwarded_up, 0);
    LONG64 du = InterlockedOr64(&s_stats.dropped_up, 0);
    LONG64 fd = InterlockedOr64(&s_stats.forwarded_down, 0);
    LONG64 dd = InterlockedOr64(&s_stats.dropped_down, 0);

    LONG64 tu = fu + du;
    LONG64 td = fd + dd;
    double up_pct   = tu > 0 ? (100.0 * (double)du / (double)tu) : 0.0;
    double down_pct = td > 0 ? (100.0 * (double)dd / (double)td) : 0.0;

    printf("[stats] up: %lld fwd / %lld drop (%.2f%% lost) | down: %lld fwd / %lld drop (%.2f%% lost) "
           "| target loss-up=%.2f%% loss-down=%.2f%%\n",
           (long long)fu, (long long)du, up_pct,
           (long long)fd, (long long)dd, down_pct,
           cfg->loss_up * 100.0, cfg->loss_down * 100.0);
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    ProxyConfig_t cfg;
    if (!s_parse_args(argc, argv, &cfg)) {
        s_print_usage(argv[0]);
        return 1;
    }

    s_rng_state = cfg.seed;
    SetConsoleCtrlHandler(s_console_handler, TRUE);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET listen_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "listen socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    BOOL connreset_disabled = FALSE;
    DWORD br = 0;
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    WSAIoctl(listen_sock, SIO_UDP_CONNRESET, &connreset_disabled, sizeof(connreset_disabled),
             NULL, 0, &br, NULL, NULL);

    struct sockaddr_in listen_addr;
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family      = AF_INET;
    listen_addr.sin_port        = htons(cfg.listen_port);
    listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed on port %u: %d\n", cfg.listen_port, WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    SOCKET upstream_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (upstream_sock == INVALID_SOCKET) {
        fprintf(stderr, "upstream socket() failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }
    WSAIoctl(upstream_sock, SIO_UDP_CONNRESET, &connreset_disabled, sizeof(connreset_disabled),
             NULL, 0, &br, NULL, NULL);

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port   = htons(cfg.target_port);
    if (inet_pton(AF_INET, cfg.target_host, &target_addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed for target host: %s\n", cfg.target_host);
        closesocket(listen_sock);
        closesocket(upstream_sock);
        WSACleanup();
        return 1;
    }

    printf("=== Kronos Lossy UDP Proxy ===\n");
    printf("Listen:    127.0.0.1:%u\n", cfg.listen_port);
    printf("Target:    %s:%u\n", cfg.target_host, cfg.target_port);
    printf("Loss up:   %.2f%%\n", cfg.loss_up * 100.0);
    printf("Loss down: %.2f%%\n", cfg.loss_down * 100.0);
    printf("Delay:     %u ms (each direction)\n", cfg.delay_ms);
    printf("Seed:      %u\n", cfg.seed);
    printf("Press Ctrl+C to stop.\n\n");

    bool client_pinned = false;
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));

    u_long nonblocking = 1;
    if (ioctlsocket(listen_sock,   FIONBIO, &nonblocking) != 0 ||
        ioctlsocket(upstream_sock, FIONBIO, &nonblocking) != 0) {
        fprintf(stderr, "ioctlsocket(FIONBIO) failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        closesocket(upstream_sock);
        WSACleanup();
        return 1;
    }

    uint64_t next_stats_ms = (uint64_t)GetTickCount64() + 1000ULL * (uint64_t)cfg.stats_interval_s;

    uint8_t buf[KRS_PROXY_BUFFER_SIZE];

    WSAPOLLFD pfds[2];
    pfds[0].fd      = listen_sock;
    pfds[0].events  = POLLRDNORM;
    pfds[1].fd      = upstream_sock;
    pfds[1].events  = POLLRDNORM;

    while (s_running) {
        pfds[0].revents = 0;
        pfds[1].revents = 0;

        int pr = WSAPoll(pfds, 2, 1);
        if (pr == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEINTR) {
                fprintf(stderr, "WSAPoll failed: %d\n", err);
                break;
            }
        }

        if (pr > 0 && (pfds[0].revents & POLLRDNORM)) {
            for (;;) {
                struct sockaddr_in src_addr;
                int src_len = sizeof(src_addr);
                int n = recvfrom(listen_sock, (char*)buf, sizeof(buf), 0,
                                 (struct sockaddr*)&src_addr, &src_len);
                if (n == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) break;
                    break;
                }
                if (n <= 0) break;

                if (!client_pinned) {
                    client_addr   = src_addr;
                    client_pinned = true;
                    char ipbuf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
                    printf("[pin] client = %s:%u\n", ipbuf, ntohs(client_addr.sin_port));
                }

                if (s_rng_uniform() < cfg.loss_up) {
                    InterlockedIncrement64(&s_stats.dropped_up);
                } else {
                    if (cfg.delay_ms > 0) Sleep(cfg.delay_ms);
                    sendto(upstream_sock, (const char*)buf, n, 0,
                           (struct sockaddr*)&target_addr, sizeof(target_addr));
                    InterlockedIncrement64(&s_stats.forwarded_up);
                }
            }
        }

        if (pr > 0 && (pfds[1].revents & POLLRDNORM) && client_pinned) {
            for (;;) {
                struct sockaddr_in srv_addr;
                int srv_len = sizeof(srv_addr);
                int m = recvfrom(upstream_sock, (char*)buf, sizeof(buf), 0,
                                 (struct sockaddr*)&srv_addr, &srv_len);
                if (m == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) break;
                    break;
                }
                if (m <= 0) break;

                if (s_rng_uniform() < cfg.loss_down) {
                    InterlockedIncrement64(&s_stats.dropped_down);
                } else {
                    if (cfg.delay_ms > 0) Sleep(cfg.delay_ms);
                    sendto(listen_sock, (const char*)buf, m, 0,
                           (struct sockaddr*)&client_addr, sizeof(client_addr));
                    InterlockedIncrement64(&s_stats.forwarded_down);
                }
            }
        }

        if (cfg.stats_interval_s > 0) {
            uint64_t now = GetTickCount64();
            if (now >= next_stats_ms) {
                s_print_stats(&cfg);
                next_stats_ms = now + 1000ULL * (uint64_t)cfg.stats_interval_s;
            }
        }
    }

    printf("\n=== Final ===\n");
    s_print_stats(&cfg);

    closesocket(listen_sock);
    closesocket(upstream_sock);
    WSACleanup();
    return 0;
}
