#include <winsock2.h>
#include <ws2tcpip.h>

#include "bench_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "ws2_32.lib")

/* HTTP/1.1 keep-alive POST/echo benchmark.
 *
 * The handler is intentionally minimal: a single connection, request bodies of
 * fixed Content-Length, replies with the same body. Methodology matches
 * bench_tcp_echo so the only added overhead vs. TCP is the per-message HTTP
 * header framing — that is the comparison point. We deliberately do not link
 * against an HTTP library; doing so would conflate framework overhead into the
 * baseline. */

#define HTTP_RECV_BUF 65536

typedef enum { ROLE_SERVER, ROLE_CLIENT } Role_e;

static volatile bool s_running = true;

static BOOL WINAPI s_console_handler(DWORD ctrl) {
    (void)ctrl;
    s_running = false;
    return TRUE;
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

static bool s_recv_full(SOCKET sock, uint8_t* buf, uint32_t need) {
    uint32_t got = 0;
    while (got < need) {
        int n = recv(sock, (char*)buf + got, (int)(need - got), 0);
        if (n <= 0) return false;
        got += (uint32_t)n;
    }
    return true;
}

/* Reads bytes into buf until "\r\n\r\n" is found. Returns the number of bytes
 * in buf at the point of return (which includes the terminator and may include
 * body bytes that arrived in the same TCP segment). headers_end_out is set to
 * the byte index immediately after the "\r\n\r\n" — i.e. the start of the body
 * region inside buf. Returns -1 on connection close or buffer exhaustion. */
static int s_read_headers(SOCKET sock, uint8_t* buf, uint32_t cap, uint32_t* headers_end_out) {
    uint32_t got = 0;
    while (got < cap) {
        int n = recv(sock, (char*)buf + got, (int)(cap - got), 0);
        if (n <= 0) return -1;
        got += (uint32_t)n;
        if (got >= 4) {
            for (uint32_t i = 0; i + 3 < got; i++) {
                if (buf[i] == '\r' && buf[i + 1] == '\n' &&
                    buf[i + 2] == '\r' && buf[i + 3] == '\n') {
                    *headers_end_out = i + 4;
                    return (int)got;
                }
            }
        }
    }
    return -1;
}

static bool s_header_value_uint(const char* headers, uint32_t headers_len,
                                const char* name, uint32_t* out) {
    size_t name_len = strlen(name);
    for (uint32_t i = 0; i + name_len < headers_len; i++) {
        if (_strnicmp((const char*)headers + i, name, name_len) == 0 &&
            i + name_len < headers_len && headers[i + name_len] == ':') {
            uint32_t j = i + (uint32_t)name_len + 1;
            while (j < headers_len && (headers[j] == ' ' || headers[j] == '\t')) j++;
            uint32_t v = 0;
            bool any = false;
            while (j < headers_len && isdigit((unsigned char)headers[j])) {
                v = v * 10 + (uint32_t)(headers[j] - '0');
                j++;
                any = true;
            }
            if (!any) return false;
            *out = v;
            return true;
        }
    }
    return false;
}

static bool s_header_contains(const char* headers, uint32_t headers_len,
                              const char* name, const char* needle) {
    size_t name_len = strlen(name);
    size_t needle_len = strlen(needle);
    for (uint32_t i = 0; i + name_len < headers_len; i++) {
        if (_strnicmp((const char*)headers + i, name, name_len) == 0 &&
            i + name_len < headers_len && headers[i + name_len] == ':') {
            uint32_t j = i + (uint32_t)name_len + 1;
            uint32_t line_end = j;
            while (line_end < headers_len &&
                   !(headers[line_end] == '\r' && line_end + 1 < headers_len &&
                     headers[line_end + 1] == '\n')) {
                line_end++;
            }
            if (line_end <= j + needle_len) return false;
            for (uint32_t k = j; k + needle_len <= line_end; k++) {
                if (_strnicmp(headers + k, needle, needle_len) == 0) return true;
            }
            return false;
        }
    }
    return false;
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

    printf("[http-echo server] Listening on TCP port %u\n", port);
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    uint8_t* buf = malloc(HTTP_RECV_BUF);
    uint8_t* resp = malloc(HTTP_RECV_BUF);
    if (!buf || !resp) {
        free(buf); free(resp);
        closesocket(listen_sock);
        return 1;
    }

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
        printf("[http-echo server] client %s:%u connected\n",
               ipbuf, ntohs(caddr.sin_port));
        fflush(stdout);

        bool keep_alive = true;
        while (s_running && keep_alive) {
            uint32_t headers_end = 0;
            int total = s_read_headers(cs, buf, HTTP_RECV_BUF, &headers_end);
            if (total < 0) break;

            uint32_t content_length = 0;
            if (!s_header_value_uint((const char*)buf, headers_end,
                                     "Content-Length", &content_length)) {
                static const char bad[] =
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n"
                    "Connection: close\r\n\r\n";
                s_send_full(cs, (const uint8_t*)bad, (uint32_t)(sizeof(bad) - 1));
                break;
            }
            keep_alive = s_header_contains((const char*)buf, headers_end,
                                           "Connection", "keep-alive");

            uint32_t body_in_buf = (uint32_t)total - headers_end;
            if (content_length > HTTP_RECV_BUF - headers_end) {
                static const char too_big[] =
                    "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\n"
                    "Connection: close\r\n\r\n";
                s_send_full(cs, (const uint8_t*)too_big, (uint32_t)(sizeof(too_big) - 1));
                break;
            }
            if (body_in_buf < content_length) {
                if (!s_recv_full(cs, buf + headers_end + body_in_buf,
                                 content_length - body_in_buf)) {
                    break;
                }
            }

            int hdr_n = snprintf((char*)resp, HTTP_RECV_BUF,
                                 "HTTP/1.1 200 OK\r\n"
                                 "Content-Length: %u\r\n"
                                 "Connection: %s\r\n"
                                 "\r\n",
                                 content_length,
                                 keep_alive ? "keep-alive" : "close");
            if (hdr_n <= 0 || (uint32_t)hdr_n + content_length > HTTP_RECV_BUF) break;
            memcpy(resp + hdr_n, buf + headers_end, content_length);
            if (!s_send_full(cs, resp, (uint32_t)hdr_n + content_length)) break;
        }

        closesocket(cs);
        printf("[http-echo server] client disconnected\n");
        fflush(stdout);
    }

    free(buf);
    free(resp);
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

    /* TCP_NODELAY: same reasoning as bench_tcp_echo — without disabling Nagle,
     * each small POST coalesces behind the 200 ms timer and the per-request RTT
     * is dominated by Nagle, not loopback. */
    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
               (const char*)&nodelay, sizeof(nodelay));

    if (msg_size < BENCH_MSG_HEADER_SIZE) msg_size = BENCH_MSG_HEADER_SIZE;

    uint8_t* req = malloc(HTTP_RECV_BUF);
    uint8_t* resp = malloc(HTTP_RECV_BUF);
    if (!req || !resp) { free(req); free(resp); closesocket(sock); return 1; }

    int hdr_n = snprintf((char*)req, HTTP_RECV_BUF,
                         "POST /echo HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Content-Length: %u\r\n"
                         "Connection: keep-alive\r\n"
                         "\r\n",
                         host, msg_size);
    if (hdr_n <= 0 || (uint32_t)hdr_n + msg_size > HTTP_RECV_BUF) {
        free(req); free(resp); closesocket(sock); return 1;
    }
    uint32_t req_total = (uint32_t)hdr_n + msg_size;
    memset(req + hdr_n, 0, msg_size);

    BenchStats_t* stats = bench_stats_create(count);
    if (!stats) { free(req); free(resp); closesocket(sock); return 1; }

    printf("[http-echo client] Target %s:%u  size=%u  warmup=%u  count=%u\n",
           host, port, msg_size, warmup, count);
    fflush(stdout);

    for (uint32_t w = 0; w < warmup; w++) {
        bench_msg_encode(req + hdr_n, (uint64_t)w, bench_time_us());
        if (!s_send_full(sock, req, req_total)) {
            fprintf(stderr, "[warmup] send failure: %d\n", WSAGetLastError());
            free(req); free(resp); bench_stats_destroy(&stats); closesocket(sock); return 1;
        }
        uint32_t headers_end = 0;
        int total = s_read_headers(sock, resp, HTTP_RECV_BUF, &headers_end);
        if (total < 0) {
            fprintf(stderr, "[warmup] header recv failure\n");
            free(req); free(resp); bench_stats_destroy(&stats); closesocket(sock); return 1;
        }
        uint32_t body_in_buf = (uint32_t)total - headers_end;
        if (body_in_buf < msg_size) {
            if (!s_recv_full(sock, resp + headers_end + body_in_buf,
                             msg_size - body_in_buf)) {
                fprintf(stderr, "[warmup] body recv failure\n");
                free(req); free(resp); bench_stats_destroy(&stats); closesocket(sock); return 1;
            }
        }
    }

    uint64_t t_start = bench_time_us();
    uint32_t recv_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t t_send = bench_time_us();
        bench_msg_encode(req + hdr_n, (uint64_t)i, t_send);
        if (!s_send_full(sock, req, req_total)) break;

        uint32_t headers_end = 0;
        int total = s_read_headers(sock, resp, HTTP_RECV_BUF, &headers_end);
        if (total < 0) break;
        uint32_t body_in_buf = (uint32_t)total - headers_end;
        if (body_in_buf < msg_size) {
            if (!s_recv_full(sock, resp + headers_end + body_in_buf,
                             msg_size - body_in_buf)) break;
        }
        uint64_t t_recv = bench_time_us();
        bench_stats_record_latency(stats, (double)(t_recv - t_send));
        recv_count++;
    }
    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    bench_stats_set_counts(stats, count, recv_count,
                           (uint64_t)count * req_total,
                           (uint64_t)recv_count * msg_size,
                           duration);

    BenchResult_t result = bench_stats_compute(stats);
    bench_stats_print(&result, "HTTP Echo Loopback - AGGREGATE", msg_size);

    free(req);
    free(resp);
    bench_stats_destroy(&stats);
    closesocket(sock);
    return 0;
}

static void s_print_usage(void) {
    printf("Usage: bench_http_echo --mode {server|client} [options]\n");
    printf("  --mode server    Run echo server on --port (default 9001)\n");
    printf("  --mode client    Run echo client against --host:--port\n");
    printf("Common options:\n");
    printf("  --host HOST      Server IP (client only, default 127.0.0.1)\n");
    printf("  --port PORT      TCP port (default 9001)\n");
    printf("  --count N        Measurement iterations (client, default 5000)\n");
    printf("  --warmup N       Warm-up iterations (client, default 500)\n");
    printf("  --size N         Body size in bytes (client, default 64, min 16)\n");
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
