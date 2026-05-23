#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

/* msquic.h was authored against MSVC SAL annotations. MinGW's sal.h covers
 * most but is missing a handful — stub the rest as no-ops before pulling
 * msquic.h in. Order matters: include sal.h first so its existing macros are
 * defined, then add the gaps. */
#include <sal.h>
#ifndef _Pre_defensive_
#define _Pre_defensive_
#endif
#ifndef _IRQL_requires_max_
#define _IRQL_requires_max_(x)
#endif
#ifndef __drv_allocatesMem
#define __drv_allocatesMem(kind)
#endif
#ifndef __drv_freesMem
#define __drv_freesMem(kind)
#endif

#include "msquic.h"
#include "bench_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "ws2_32.lib")

/* QUIC loopback echo benchmark using msquic.
 *
 * Comparison point against bench_tcp_echo / bench_udp_echo for the thesis.
 * Single client, single bidirectional stream per measured iteration. The
 * connection (TLS+transport handshake) is established during warm-up and
 * therefore is NOT part of the measured RTT — only the stream-level
 * open/send/echo/close cycle is.
 *
 * TLS overhead is intentional and explicitly noted as a real difference
 * vs. the TCP/UDP baselines: it is part of the QUIC story. */

#define ALPN "kr-echo"
#define DEFAULT_PORT 9001
static const char IDLE_TIMEOUT_MS = 0; /* unused, kept for clarity */

typedef enum { ROLE_SERVER, ROLE_CLIENT } Role_e;

static const QUIC_API_TABLE* MsQuic = NULL;
static HQUIC                 Registration = NULL;
static HQUIC                 Configuration = NULL;
static volatile bool         s_running = true;

/* ───────────────────────────── helpers ───────────────────────────── */

static BOOL WINAPI s_console_handler(DWORD ctrl) {
    (void)ctrl;
    s_running = false;
    return TRUE;
}

static bool s_hex_to_bytes(const char* hex, uint8_t* out, size_t out_len) {
    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) return false;
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return false;
        out[i] = (uint8_t)byte;
    }
    return true;
}

static bool s_open_msquic(void) {
    QUIC_STATUS s = MsQuicOpen2(&MsQuic);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "MsQuicOpen2 failed: 0x%x\n", (unsigned)s);
        return false;
    }
    s = MsQuic->RegistrationOpen(NULL, &Registration);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "RegistrationOpen failed: 0x%x\n", (unsigned)s);
        MsQuicClose(MsQuic);
        MsQuic = NULL;
        return false;
    }
    return true;
}

static void s_close_msquic(void) {
    if (Configuration) { MsQuic->ConfigurationClose(Configuration); Configuration = NULL; }
    if (Registration)  { MsQuic->RegistrationClose(Registration);   Registration  = NULL; }
    if (MsQuic)        { MsQuicClose(MsQuic);                       MsQuic        = NULL; }
}

static QUIC_BUFFER s_alpn = { (uint32_t)(sizeof(ALPN) - 1), (uint8_t*)ALPN };

static bool s_load_server_config(const char* cert_hash_hex) {
    QUIC_SETTINGS settings = {0};
    settings.IdleTimeoutMs            = 60000;
    settings.IsSet.IdleTimeoutMs      = TRUE;
    settings.PeerBidiStreamCount      = 100;
    settings.IsSet.PeerBidiStreamCount = TRUE;
    settings.ServerResumptionLevel    = 0;
    settings.IsSet.ServerResumptionLevel = TRUE;

    QUIC_STATUS s = MsQuic->ConfigurationOpen(Registration, &s_alpn, 1,
                                              &settings, sizeof(settings),
                                              NULL, &Configuration);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "Server ConfigurationOpen failed: 0x%x\n", (unsigned)s);
        return false;
    }

    QUIC_CERTIFICATE_HASH_STORE hash_store = {0};
    hash_store.Flags = QUIC_CERTIFICATE_HASH_STORE_FLAG_NONE; /* CurrentUser store */
    if (!s_hex_to_bytes(cert_hash_hex, hash_store.ShaHash, sizeof(hash_store.ShaHash))) {
        fprintf(stderr, "Bad --cert-hash; expected 40 hex chars (SHA1 thumbprint).\n");
        return false;
    }
    strncpy(hash_store.StoreName, "My", sizeof(hash_store.StoreName) - 1);

    QUIC_CREDENTIAL_CONFIG cred = {0};
    cred.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH_STORE;
    cred.Flags = QUIC_CREDENTIAL_FLAG_NONE;
    cred.CertificateHashStore = &hash_store;

    s = MsQuic->ConfigurationLoadCredential(Configuration, &cred);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "ConfigurationLoadCredential failed: 0x%x. Is the cert "
                        "thumbprint correct and present in CurrentUser\\My?\n",
                (unsigned)s);
        return false;
    }
    return true;
}

static bool s_load_client_config(void) {
    QUIC_SETTINGS settings = {0};
    settings.IdleTimeoutMs       = 60000;
    settings.IsSet.IdleTimeoutMs = TRUE;

    QUIC_STATUS s = MsQuic->ConfigurationOpen(Registration, &s_alpn, 1,
                                              &settings, sizeof(settings),
                                              NULL, &Configuration);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "Client ConfigurationOpen failed: 0x%x\n", (unsigned)s);
        return false;
    }

    QUIC_CREDENTIAL_CONFIG cred = {0};
    cred.Type  = QUIC_CREDENTIAL_TYPE_NONE;
    cred.Flags = QUIC_CREDENTIAL_FLAG_CLIENT |
                 QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

    s = MsQuic->ConfigurationLoadCredential(Configuration, &cred);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "Client ConfigurationLoadCredential failed: 0x%x\n",
                (unsigned)s);
        return false;
    }
    return true;
}

/* ───────────────────────────── server ───────────────────────────── */

typedef struct ServerStreamCtx {
    QUIC_BUFFER tx_buf;       /* points into a heap-owned echo buffer */
    uint8_t*    echo_storage; /* freed in SEND_COMPLETE */
} ServerStreamCtx;

static QUIC_STATUS QUIC_API s_server_stream_cb(HQUIC stream, void* ctx, QUIC_STREAM_EVENT* ev) {
    ServerStreamCtx* sctx = (ServerStreamCtx*)ctx;
    switch (ev->Type) {
        case QUIC_STREAM_EVENT_RECEIVE: {
            uint64_t total = ev->RECEIVE.TotalBufferLength;
            if (total > 0) {
                uint8_t* echo = (uint8_t*)malloc((size_t)total);
                if (!echo) return QUIC_STATUS_OUT_OF_MEMORY;
                uint64_t off = 0;
                for (uint32_t i = 0; i < ev->RECEIVE.BufferCount; i++) {
                    memcpy(echo + off, ev->RECEIVE.Buffers[i].Buffer, ev->RECEIVE.Buffers[i].Length);
                    off += ev->RECEIVE.Buffers[i].Length;
                }
                ServerStreamCtx* tx = (ServerStreamCtx*)malloc(sizeof(*tx));
                if (!tx) { free(echo); return QUIC_STATUS_OUT_OF_MEMORY; }
                tx->echo_storage = echo;
                tx->tx_buf.Buffer = echo;
                tx->tx_buf.Length = (uint32_t)total;
                QUIC_SEND_FLAGS sf = (ev->RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN)
                                     ? QUIC_SEND_FLAG_FIN : QUIC_SEND_FLAG_NONE;
                MsQuic->StreamSend(stream, &tx->tx_buf, 1, sf, tx);
            }
            return QUIC_STATUS_SUCCESS;
        }
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            ServerStreamCtx* tx = (ServerStreamCtx*)ev->SEND_COMPLETE.ClientContext;
            if (tx) { free(tx->echo_storage); free(tx); }
            return QUIC_STATUS_SUCCESS;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            MsQuic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
            return QUIC_STATUS_SUCCESS;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            if (sctx) free(sctx);
            MsQuic->StreamClose(stream);
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static QUIC_STATUS QUIC_API s_server_conn_cb(HQUIC conn, void* ctx, QUIC_CONNECTION_EVENT* ev) {
    (void)ctx;
    switch (ev->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            MsQuic->ConnectionSendResumptionTicket(conn, QUIC_SEND_RESUMPTION_FLAG_NONE, 0, NULL);
            return QUIC_STATUS_SUCCESS;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
            ServerStreamCtx* sctx = (ServerStreamCtx*)calloc(1, sizeof(*sctx));
            MsQuic->SetCallbackHandler(ev->PEER_STREAM_STARTED.Stream, s_server_stream_cb, sctx);
            return QUIC_STATUS_SUCCESS;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            MsQuic->ConnectionClose(conn);
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static QUIC_STATUS QUIC_API s_server_listener_cb(HQUIC listener, void* ctx, QUIC_LISTENER_EVENT* ev) {
    (void)listener; (void)ctx;
    if (ev->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
        MsQuic->SetCallbackHandler(ev->NEW_CONNECTION.Connection, s_server_conn_cb, NULL);
        return MsQuic->ConnectionSetConfiguration(ev->NEW_CONNECTION.Connection, Configuration);
    }
    return QUIC_STATUS_NOT_SUPPORTED;
}

static int s_run_server(uint16_t port, const char* cert_hash_hex) {
    if (!s_open_msquic()) return 1;
    if (!s_load_server_config(cert_hash_hex)) { s_close_msquic(); return 1; }

    HQUIC listener = NULL;
    QUIC_STATUS s = MsQuic->ListenerOpen(Registration, s_server_listener_cb, NULL, &listener);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "ListenerOpen failed: 0x%x\n", (unsigned)s);
        s_close_msquic();
        return 1;
    }

    QUIC_ADDR addr = {0};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_INET);
    QuicAddrSetPort(&addr, port);

    s = MsQuic->ListenerStart(listener, &s_alpn, 1, &addr);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "ListenerStart failed: 0x%x\n", (unsigned)s);
        MsQuic->ListenerClose(listener);
        s_close_msquic();
        return 1;
    }

    printf("[quic-echo server] Listening on UDP port %u\n", port);
    printf("Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (s_running) Sleep(200);

    MsQuic->ListenerStop(listener);
    MsQuic->ListenerClose(listener);
    s_close_msquic();
    return 0;
}

/* ───────────────────────────── client ───────────────────────────── */

typedef struct ClientCtx {
    HANDLE   ev_connected;
    HANDLE   ev_failed;
    QUIC_STATUS connect_status;
} ClientCtx;

typedef struct ClientStreamCtx {
    HANDLE        ev_done;
    uint8_t*      recv_buf;
    uint32_t      recv_capacity;
    uint64_t      recv_total;
    uint64_t      expect;
    QUIC_BUFFER   send_buf;     /* lifetime: until SEND_COMPLETE */
    bool          aborted;
} ClientStreamCtx;

static QUIC_STATUS QUIC_API s_client_stream_cb(HQUIC stream, void* ctx, QUIC_STREAM_EVENT* ev) {
    ClientStreamCtx* sc = (ClientStreamCtx*)ctx;
    switch (ev->Type) {
        case QUIC_STREAM_EVENT_RECEIVE: {
            uint64_t off = 0;
            for (uint32_t i = 0; i < ev->RECEIVE.BufferCount; i++) {
                uint32_t copy = ev->RECEIVE.Buffers[i].Length;
                if (sc->recv_total + off + copy <= sc->recv_capacity) {
                    memcpy(sc->recv_buf + sc->recv_total + off,
                           ev->RECEIVE.Buffers[i].Buffer, copy);
                }
                off += copy;
            }
            sc->recv_total += off;
            if (sc->recv_total >= sc->expect) {
                SetEvent(sc->ev_done);
            }
            return QUIC_STATUS_SUCCESS;
        }
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            return QUIC_STATUS_SUCCESS;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            MsQuic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
            return QUIC_STATUS_SUCCESS;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        case QUIC_STREAM_EVENT_PEER_RECEIVE_ABORTED:
            sc->aborted = true;
            SetEvent(sc->ev_done);
            return QUIC_STATUS_SUCCESS;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static QUIC_STATUS QUIC_API s_client_conn_cb(HQUIC conn, void* ctx, QUIC_CONNECTION_EVENT* ev) {
    (void)conn;
    ClientCtx* cc = (ClientCtx*)ctx;
    switch (ev->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            cc->connect_status = QUIC_STATUS_SUCCESS;
            SetEvent(cc->ev_connected);
            return QUIC_STATUS_SUCCESS;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            cc->connect_status = ev->SHUTDOWN_INITIATED_BY_TRANSPORT.Status;
            SetEvent(cc->ev_failed);
            return QUIC_STATUS_SUCCESS;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            return QUIC_STATUS_SUCCESS;
        default:
            return QUIC_STATUS_SUCCESS;
    }
}

static int s_do_one_iteration(HQUIC conn, uint8_t* tx_buf, uint8_t* rx_buf, uint32_t size,
                              uint64_t* rtt_us_out) {
    HQUIC stream = NULL;
    ClientStreamCtx sc = {0};
    sc.ev_done       = CreateEventA(NULL, TRUE, FALSE, NULL);
    sc.recv_buf      = rx_buf;
    sc.recv_capacity = size;
    sc.recv_total    = 0;
    sc.expect        = size;
    sc.send_buf.Buffer = tx_buf;
    sc.send_buf.Length = size;
    sc.aborted       = false;

    if (!sc.ev_done) return 1;

    uint64_t t_send = bench_time_us();
    bench_msg_encode(tx_buf, 0, t_send);

    QUIC_STATUS s = MsQuic->StreamOpen(conn, QUIC_STREAM_OPEN_FLAG_NONE,
                                       s_client_stream_cb, &sc, &stream);
    if (QUIC_FAILED(s)) { CloseHandle(sc.ev_done); return 1; }
    s = MsQuic->StreamStart(stream, QUIC_STREAM_START_FLAG_IMMEDIATE);
    if (QUIC_FAILED(s)) { MsQuic->StreamClose(stream); CloseHandle(sc.ev_done); return 1; }
    s = MsQuic->StreamSend(stream, &sc.send_buf, 1, QUIC_SEND_FLAG_FIN, NULL);
    if (QUIC_FAILED(s)) { MsQuic->StreamClose(stream); CloseHandle(sc.ev_done); return 1; }

    DWORD wait = WaitForSingleObject(sc.ev_done, 5000);
    uint64_t t_recv = bench_time_us();

    /* Stream cleanup: graceful close on send, abort the recv side which we
     * ignore beyond the echo. The SHUTDOWN_COMPLETE callback runs in the
     * MsQuic thread and frees the stream handle internally; we just call
     * StreamClose here to release our reference. */
    MsQuic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT_RECEIVE, 0);
    MsQuic->StreamClose(stream);
    CloseHandle(sc.ev_done);

    if (wait != WAIT_OBJECT_0 || sc.aborted || sc.recv_total < size) return 1;

    *rtt_us_out = t_recv - t_send;
    return 0;
}

static int s_run_client(const char* host, uint16_t port, uint32_t count,
                        uint32_t msg_size, uint32_t warmup) {
    if (!s_open_msquic()) return 1;
    if (!s_load_client_config()) { s_close_msquic(); return 1; }

    if (msg_size < BENCH_MSG_HEADER_SIZE) msg_size = BENCH_MSG_HEADER_SIZE;

    ClientCtx cc = {0};
    cc.ev_connected = CreateEventA(NULL, TRUE, FALSE, NULL);
    cc.ev_failed    = CreateEventA(NULL, TRUE, FALSE, NULL);

    HQUIC conn = NULL;
    QUIC_STATUS s = MsQuic->ConnectionOpen(Registration, s_client_conn_cb, &cc, &conn);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "ConnectionOpen failed: 0x%x\n", (unsigned)s);
        CloseHandle(cc.ev_connected); CloseHandle(cc.ev_failed);
        s_close_msquic();
        return 1;
    }
    s = MsQuic->ConnectionStart(conn, Configuration, QUIC_ADDRESS_FAMILY_INET, host, port);
    if (QUIC_FAILED(s)) {
        fprintf(stderr, "ConnectionStart failed: 0x%x\n", (unsigned)s);
        MsQuic->ConnectionClose(conn);
        CloseHandle(cc.ev_connected); CloseHandle(cc.ev_failed);
        s_close_msquic();
        return 1;
    }

    HANDLE waits[2] = {cc.ev_connected, cc.ev_failed};
    DWORD which = WaitForMultipleObjects(2, waits, FALSE, 10000);
    if (which != WAIT_OBJECT_0) {
        fprintf(stderr, "Connect failed or timed out (status 0x%x)\n",
                (unsigned)cc.connect_status);
        MsQuic->ConnectionShutdown(conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        MsQuic->ConnectionClose(conn);
        CloseHandle(cc.ev_connected); CloseHandle(cc.ev_failed);
        s_close_msquic();
        return 1;
    }

    uint8_t* tx = (uint8_t*)calloc(1, msg_size);
    uint8_t* rx = (uint8_t*)calloc(1, msg_size);
    BenchStats_t* stats = bench_stats_create(count);
    if (!tx || !rx || !stats) {
        free(tx); free(rx); bench_stats_destroy(&stats);
        MsQuic->ConnectionShutdown(conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        MsQuic->ConnectionClose(conn);
        CloseHandle(cc.ev_connected); CloseHandle(cc.ev_failed);
        s_close_msquic();
        return 1;
    }

    printf("[quic-echo client] Target %s:%u  size=%u  warmup=%u  count=%u\n",
           host, port, msg_size, warmup, count);
    fflush(stdout);

    for (uint32_t w = 0; w < warmup; w++) {
        uint64_t rtt;
        if (s_do_one_iteration(conn, tx, rx, msg_size, &rtt) != 0) {
            fprintf(stderr, "[warmup] iteration failed\n");
            free(tx); free(rx); bench_stats_destroy(&stats);
            MsQuic->ConnectionShutdown(conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
            MsQuic->ConnectionClose(conn);
            CloseHandle(cc.ev_connected); CloseHandle(cc.ev_failed);
            s_close_msquic();
            return 1;
        }
    }

    uint64_t t_start = bench_time_us();
    uint32_t recv_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t rtt;
        if (s_do_one_iteration(conn, tx, rx, msg_size, &rtt) != 0) break;
        bench_stats_record_latency(stats, (double)rtt);
        recv_count++;
    }
    uint64_t t_end = bench_time_us();
    double duration = (double)(t_end - t_start) / 1000000.0;

    bench_stats_set_counts(stats, count, recv_count,
                           (uint64_t)count * msg_size,
                           (uint64_t)recv_count * msg_size,
                           duration);

    BenchResult_t result = bench_stats_compute(stats);
    bench_stats_print(&result, "QUIC Echo Loopback - AGGREGATE", msg_size);

    MsQuic->ConnectionShutdown(conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    MsQuic->ConnectionClose(conn);
    CloseHandle(cc.ev_connected); CloseHandle(cc.ev_failed);
    free(tx); free(rx);
    bench_stats_destroy(&stats);
    s_close_msquic();
    return 0;
}

/* ───────────────────────────── main ───────────────────────────── */

static void s_print_usage(void) {
    printf("Usage: bench_quic_echo --mode {server|client} [options]\n");
    printf("  --mode server   --cert-hash <40-hex SHA1 thumbprint of cert in CurrentUser\\My>\n");
    printf("  --mode client   --host <ip> --port <port> --count N --warmup N --size N\n");
    printf("Common: --port (default 9001), --host (default 127.0.0.1)\n");
}

int main(int argc, char* argv[]) {
    Role_e role = ROLE_CLIENT;
    bool role_set = false;
    const char* host = "127.0.0.1";
    uint16_t port = DEFAULT_PORT;
    uint32_t count = 5000;
    uint32_t warmup = 500;
    uint32_t size = 64;
    const char* cert_hash = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            i++;
            if      (strcmp(argv[i], "server") == 0) { role = ROLE_SERVER; role_set = true; }
            else if (strcmp(argv[i], "client") == 0) { role = ROLE_CLIENT; role_set = true; }
            else { s_print_usage(); return 1; }
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
        } else if (strcmp(argv[i], "--cert-hash") == 0 && i + 1 < argc) {
            cert_hash = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            s_print_usage(); return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            s_print_usage(); return 1;
        }
    }

    if (!role_set) { s_print_usage(); return 1; }
    if (role == ROLE_SERVER && !cert_hash) {
        fprintf(stderr, "Server mode requires --cert-hash. Run setup_quic_cert.ps1 first.\n");
        return 1;
    }

    bench_timer_init();
    SetConsoleCtrlHandler(s_console_handler, TRUE);

    return (role == ROLE_SERVER)
               ? s_run_server(port, cert_hash)
               : s_run_client(host, port, count, size, warmup);
}
