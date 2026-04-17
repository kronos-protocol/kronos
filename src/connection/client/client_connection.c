#include "kronos_client.h"
#include "kronos.h"
#include "kronos_fragment.h"
#include "kronos_packet_counter.h"
#include "kronos_ack.h"
#include "kronos_congestion.h"

#include "client_internal.h"
#include "network_internal.h"
#include "frame_metadata.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>


static UDPSocketRef_t s_create_client_socket(void) {
    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    DWORD off = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    BOOL connreset_disabled = FALSE;
    DWORD bytes_returned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &connreset_disabled, sizeof(connreset_disabled),
             NULL, 0, &bytes_returned, NULL, NULL);

    struct sockaddr_in6 bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_port = 0;
    bind_addr.sin6_addr = in6addr_any;

    if (bind(s, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

ServerConnection_t* krs_client_server_connect(PortAddress_t server_address) {
    krs_wsa_init();

    UDPSocketRef_t sock = s_create_client_socket();
    if (sock == INVALID_SOCKET) {
        krs_wsa_cleanup();
        return NULL;
    }

    FrameBuilder_c* builder = krs_frame_builder_create(0, CONNECTION);
    if (!builder) {
        closesocket(sock);
        krs_wsa_cleanup();
        return NULL;
    }

    uint8_t buf[64];
    uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
    krs_frame_builder_destroy(&builder);

    if (n == 0) {
        closesocket(sock);
        krs_wsa_cleanup();
        return NULL;
    }

    WSABUF wsabuf;
    wsabuf.len = n;
    wsabuf.buf = (char*)buf;
    DWORD sent;
    if (WSASendTo(sock, &wsabuf, 1, &sent, 0,
                  (const struct sockaddr*)&server_address, sizeof(server_address),
                  NULL, NULL) == SOCKET_ERROR) {
        closesocket(sock);
        krs_wsa_cleanup();
        return NULL;
    }

    DWORD timeout_ms = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));

    uint8_t recv_buf[128];
    struct sockaddr_in6 from_addr;
    int from_len = sizeof(from_addr);
    int recv_bytes = recvfrom(sock, (char*)recv_buf, sizeof(recv_buf), 0,
                              (struct sockaddr*)&from_addr, &from_len);
    if (recv_bytes < 0) {
        closesocket(sock);
        krs_wsa_cleanup();
        return NULL;
    }

    uint8_t body_buf[32];
    Frame_t frame = krs_frame_create(recv_buf, (uint16_t)recv_bytes, body_buf, sizeof(body_buf));
    if (frame.protocol_char != 0x4B || frame.frame_type != SOCKET_ACK || frame.body_length < 4) {
        closesocket(sock);
        krs_wsa_cleanup();
        return NULL;
    }

    uint32_t connection_id = ((uint32_t)frame.body[0] << 24) |
                             ((uint32_t)frame.body[1] << 16) |
                             ((uint32_t)frame.body[2] << 8) |
                             (uint32_t)frame.body[3];

    ServerConnection_t* conn = malloc(sizeof(ServerConnection_t));
    if (!conn) {
        closesocket(sock);
        krs_wsa_cleanup();
        return NULL;
    }

    conn->socket = sock;
    conn->server_address = server_address;
    InitializeCriticalSection(&conn->state_lock);
    conn->connection_id = connection_id;
    conn->connected = true;
    conn->packet_counter = krs_packet_counter_create();
    conn->recv_thread = NULL;
    conn->running = false;
    conn->callback = NULL;
    conn->callback_user_data = NULL;
    conn->reassembler = NULL;
    conn->ack_tracker = NULL;
    conn->congestion = NULL;
    conn->last_heartbeat_sent_ms = GetTickCount64();
    return conn;
}

static void s_client_send_frame(SOCKET sock, const PortAddress_t* addr,
                                const uint8_t* frame_data, uint16_t frame_size) {
    WSABUF wsabuf;
    wsabuf.len = frame_size;
    wsabuf.buf = (char*)frame_data;
    DWORD sent;
    WSASendTo(sock, &wsabuf, 1, &sent, 0,
              (const struct sockaddr*)addr, sizeof(*addr), NULL, NULL);
}

Void_r krs_client_send(ServerConnection_t* conn, Channel_t channel,
                       const uint8_t* data, uint16_t length, bool require_ack) {
    Void_r result = {0};

    if (!conn || !data) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "conn or data is NULL");
        return result;
    }
    if (!conn->connected) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_CLIENT_NOT_CONNECTED, "not connected");
        return result;
    }

    EnterCriticalSection(&conn->state_lock);
    uint64_t pid = conn->packet_counter ? krs_packet_counter_next(conn->packet_counter, channel) : 0;
    LeaveCriticalSection(&conn->state_lock);

    if (require_ack) {
        EnterCriticalSection(&conn->state_lock);
        if (!conn->congestion) {
            conn->congestion = krs_congestion_create();
        }
        if (conn->congestion && !krs_congestion_can_send(conn->congestion)) {
            LeaveCriticalSection(&conn->state_lock);
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_CONGESTION_WINDOW_FULL,
                                                           "congestion window full");
            return result;
        }
        if (conn->congestion) {
            krs_congestion_on_send(conn->congestion);
        }
        LeaveCriticalSection(&conn->state_lock);
    }

    if (length <= KRS_MAX_PAYLOAD_PER_FRAGMENT) {
        FrameBuilder_c* builder = krs_frame_builder_create(channel, BASIC_MESSAGE);
        if (!builder) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "builder alloc failed");
            return result;
        }

        krs_frame_builder_set_packet_id(builder, pid);
        if (require_ack) {
            krs_frame_builder_set_flag(builder, META_FLAG_ACK_REQUIRED);
        }
        krs_frame_builder_set_data(builder, data, length);

        uint8_t buf[KRONOS_BUFFER_SIZE];
        uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
        krs_frame_builder_destroy(&builder);

        if (n == 0) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_BUFFER_TOO_SMALL, "serialize failed");
            return result;
        }

        if (require_ack) {
            EnterCriticalSection(&conn->state_lock);
            if (!conn->ack_tracker) {
                conn->ack_tracker = krs_ack_tracker_create(1000, 5);
            }
            if (conn->ack_tracker) {
                krs_ack_tracker_expect(conn->ack_tracker, pid, buf, n);
            }
            LeaveCriticalSection(&conn->state_lock);
        }

        s_client_send_frame(conn->socket, &conn->server_address, buf, n);
    } else {
        if (require_ack) {
            EnterCriticalSection(&conn->state_lock);
            if (!conn->ack_tracker) {
                conn->ack_tracker = krs_ack_tracker_create(1000, 5);
            }
            LeaveCriticalSection(&conn->state_lock);
        }

        FragmentResult_t frag = krs_fragment_split(channel, BASIC_MESSAGE, pid,
                                                    data, length, KRS_DEFAULT_MTU,
                                                    (uint16_t)(require_ack ? (1u << META_FLAG_ACK_REQUIRED) : 0));
        if (frag.base.error_code != KRS_SUCCESS) {
            result.base = frag.base;
            return result;
        }

        if (require_ack && conn->ack_tracker) {
            EnterCriticalSection(&conn->state_lock);
            for (uint16_t f = 0; f < frag.fragment_count; f++) {
                krs_ack_tracker_expect(conn->ack_tracker, pid, frag.fragments[f], frag.fragment_sizes[f]);
            }
            LeaveCriticalSection(&conn->state_lock);
        }

        for (uint16_t f = 0; f < frag.fragment_count; f++) {
            s_client_send_frame(conn->socket, &conn->server_address,
                                frag.fragments[f], frag.fragment_sizes[f]);
        }
        krs_fragment_result_destroy(&frag);
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_client_send_blocking(ServerConnection_t* conn, Channel_t channel,
                                const uint8_t* data, uint16_t length, uint32_t timeout_ms) {
    uint64_t deadline = GetTickCount64() + timeout_ms;

    for (;;) {
        Void_r result = krs_client_send(conn, channel, data, length, true);
        if (result.base.valid) return result;
        if (result.base.error_code != KRS_ERR_SERVER_CONGESTION_WINDOW_FULL) return result;
        if (GetTickCount64() >= deadline) return result;
        Sleep(1);
    }
}

void krs_client_set_callback(ServerConnection_t* conn, ChannelMessageCallback_f callback,
                             void* user_data) {
    if (!conn) return;
    conn->callback = callback;
    conn->callback_user_data = user_data;
}

static void s_client_send_ack(SOCKET sock, const PortAddress_t* addr,
                              Channel_t channel, uint64_t packet_id) {
    FrameBuilder_c* builder = krs_frame_builder_create(channel, MESSAGE_ACK);
    if (!builder) return;
    krs_frame_builder_set_packet_id(builder, packet_id);
    uint8_t buf[64];
    uint16_t n = krs_frame_builder_serialize(builder, buf, sizeof(buf));
    krs_frame_builder_destroy(&builder);
    if (n == 0) return;

    WSABUF wsabuf;
    wsabuf.len = n;
    wsabuf.buf = (char*)buf;
    DWORD sent;
    WSASendTo(sock, &wsabuf, 1, &sent, 0,
              (const struct sockaddr*)addr, sizeof(*addr), NULL, NULL);
}

#define KRS_CLIENT_HEARTBEAT_INTERVAL_MS 5000

static DWORD WINAPI s_client_recv_thread(LPVOID param) {
    ServerConnection_t* conn = (ServerConnection_t*)param;

    DWORD timeout_ms = 100;
    setsockopt(conn->socket, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&timeout_ms, sizeof(timeout_ms));

    while (conn->running) {
        uint64_t now = GetTickCount64();
        if (now - conn->last_heartbeat_sent_ms >= KRS_CLIENT_HEARTBEAT_INTERVAL_MS) {
            FrameBuilder_c* hb_builder = krs_frame_builder_create(1, HEARTBEAT);
            if (hb_builder) {
                uint8_t hb_buf[32];
                uint16_t hb_n = krs_frame_builder_serialize(hb_builder, hb_buf, sizeof(hb_buf));
                krs_frame_builder_destroy(&hb_builder);
                if (hb_n > 0) {
                    WSABUF hb_wsabuf;
                    hb_wsabuf.len = hb_n;
                    hb_wsabuf.buf = (char*)hb_buf;
                    DWORD hb_sent;
                    WSASendTo(conn->socket, &hb_wsabuf, 1, &hb_sent, 0,
                              (const struct sockaddr*)&conn->server_address,
                              sizeof(conn->server_address), NULL, NULL);
                }
            }
            conn->last_heartbeat_sent_ms = now;
        }

        EnterCriticalSection(&conn->state_lock);
        if (conn->ack_tracker) {
            uint64_t retry_ids[16];
            uint32_t retry_count = krs_ack_tracker_check_timeouts(conn->ack_tracker, retry_ids, 16);
            uint32_t to_resend = retry_count < 16 ? retry_count : 16;

            if (to_resend > 0 && conn->congestion) {
                krs_congestion_on_loss(conn->congestion);
            }

            uint8_t retry_bufs[16][KRONOS_BUFFER_SIZE];
            uint16_t retry_sizes[16];
            uint32_t actual_retries = 0;

            for (uint32_t r = 0; r < to_resend; r++) {
                uint16_t frame_size = 0;
                const uint8_t* frame_data = krs_ack_tracker_get_retry_frame(
                    conn->ack_tracker, retry_ids[r], &frame_size);
                if (frame_data && frame_size > 0 && frame_size <= KRONOS_BUFFER_SIZE) {
                    memcpy(retry_bufs[actual_retries], frame_data, frame_size);
                    retry_sizes[actual_retries] = frame_size;
                    actual_retries++;
                }
            }
            LeaveCriticalSection(&conn->state_lock);

            for (uint32_t r = 0; r < actual_retries; r++) {
                s_client_send_frame(conn->socket, &conn->server_address,
                                    retry_bufs[r], retry_sizes[r]);
            }
        } else {
            LeaveCriticalSection(&conn->state_lock);
        }

        uint8_t raw[KRONOS_BUFFER_SIZE];
        struct sockaddr_in6 from;
        int from_len = sizeof(from);
        int n = recvfrom(conn->socket, (char*)raw, sizeof(raw), 0,
                         (struct sockaddr*)&from, &from_len);
        if (n <= 0) continue;

        uint8_t body_buf[KRONOS_BUFFER_SIZE];
        Frame_t frame = krs_frame_create(raw, (uint16_t)n, body_buf, sizeof(body_buf));
        if (frame.protocol_char != 0x4B) continue;
        if (frame.body_length > 0 && frame.body == NULL) continue;

        if (frame.frame_type == MESSAGE_ACK) {
            EnterCriticalSection(&conn->state_lock);
            double rtt_ms = -1.0;
            if (conn->ack_tracker) {
                rtt_ms = krs_ack_tracker_receive_rtt(conn->ack_tracker, frame.packet_id);
            }
            if (rtt_ms >= 0.0 && conn->congestion) {
                krs_congestion_on_ack(conn->congestion, rtt_ms);
                if (conn->ack_tracker) {
                    krs_ack_tracker_set_timeout(conn->ack_tracker, (uint32_t)krs_congestion_get_rto(conn->congestion));
                }
            }
            LeaveCriticalSection(&conn->state_lock);
            continue;
        }
        if (frame.channel < 10) continue;

        bool has_fragment_info = (frame.presence_flags & (uint16_t)(1u << META_FLAG_FRAGMENT_INFO)) != 0;

        if (!has_fragment_info) {
            bool ack_required = (frame.presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) != 0;
            if (ack_required) {
                s_client_send_ack(conn->socket, &conn->server_address,
                                  frame.channel, frame.packet_id);
            }
            if (conn->callback) {
                conn->callback(frame.channel, OPEN_CHANNEL, conn->connection_id,
                               frame.body, frame.body_length,
                               conn->callback_user_data);
            }
        } else {
            if (!conn->reassembler) {
                conn->reassembler = krs_reassembler_create();
                if (!conn->reassembler) continue;
            }

            ReassembleResult_t reassembly = krs_reassembler_feed(conn->reassembler, &frame);
            if (!reassembly.complete) continue;

            if (reassembly.ack_required) {
                s_client_send_ack(conn->socket, &conn->server_address,
                                  frame.channel, frame.packet_id);
            }

            if (conn->callback) {
                conn->callback(frame.channel, OPEN_CHANNEL, conn->connection_id,
                               reassembly.data, (uint16_t)reassembly.data_length,
                               conn->callback_user_data);
            }

            free(reassembly.data);
        }
    }

    return 0;
}

Void_r krs_client_start_receive(ServerConnection_t* conn) {
    Void_r result = {0};
    if (!conn) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "conn is NULL");
        return result;
    }
    if (!conn->connected) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_CLIENT_NOT_CONNECTED, "not connected");
        return result;
    }
    if (conn->recv_thread) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_ALREADY_INITIALIZED, "already receiving");
        return result;
    }

    conn->running = true;
    conn->recv_thread = CreateThread(NULL, 0, s_client_recv_thread, conn, 0, NULL);
    if (!conn->recv_thread) {
        conn->running = false;
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET, "CreateThread failed");
        return result;
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
}

void krs_client_disconnect(ServerConnection_t** conn) {
    if (!conn || !*conn) return;

    if ((*conn)->connected && (*conn)->socket != INVALID_SOCKET) {
        FrameBuilder_c* dc_builder = krs_frame_builder_create(0, DISCONNECT);
        if (dc_builder) {
            uint8_t dc_buf[32];
            uint16_t dc_n = krs_frame_builder_serialize(dc_builder, dc_buf, sizeof(dc_buf));
            krs_frame_builder_destroy(&dc_builder);
            if (dc_n > 0) {
                WSABUF dc_wsabuf;
                dc_wsabuf.len = dc_n;
                dc_wsabuf.buf = (char*)dc_buf;
                DWORD dc_sent;
                WSASendTo((*conn)->socket, &dc_wsabuf, 1, &dc_sent, 0,
                          (const struct sockaddr*)&(*conn)->server_address,
                          sizeof((*conn)->server_address), NULL, NULL);
            }
        }
        (*conn)->connected = false;
    }

    if ((*conn)->recv_thread) {
        (*conn)->running = false;
        WaitForSingleObject((*conn)->recv_thread, 2000);
        CloseHandle((*conn)->recv_thread);
        (*conn)->recv_thread = NULL;
    }
    krs_reassembler_destroy(&(*conn)->reassembler);
    krs_congestion_destroy(&(*conn)->congestion);
    krs_ack_tracker_destroy(&(*conn)->ack_tracker);
    krs_packet_counter_destroy(&(*conn)->packet_counter);
    if ((*conn)->socket != INVALID_SOCKET) {
        closesocket((*conn)->socket);
    }
    DeleteCriticalSection(&(*conn)->state_lock);
    free(*conn);
    *conn = NULL;
    krs_wsa_cleanup();
}
