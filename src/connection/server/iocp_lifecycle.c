#include "kronos_server.h"
#include "kronos_log.h"

#include "server_internal.h"
#include "iocp_internal.h"
#include "message_pool_internal.h"

#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <mswsock.h>


#define SHUTDOWN_KEY ((ULONG_PTR)(~(ULONG_PTR)0))

static void s_post_initial_recv(HANDLE iocp, UDPSocketDescriptor_t* desc) {
    UDPOverlapped_t* ctx = calloc(1, sizeof(UDPOverlapped_t));
    if (!ctx) {
        KRS_LOG_ERROR("iocp", "recv context allocation failed for port %u", desc->port);
        return;
    }

    ctx->port = desc->port;
    ctx->socket = desc->udp_socket_ref;
    ctx->wsabuf.buf = (char*)ctx->buffer;
    ctx->wsabuf.len = sizeof(ctx->buffer);
    ctx->remote_addr_len = sizeof(ctx->remote_addr);
    ctx->flags = 0;

    desc->recv_ctx = ctx;

    HANDLE associated = CreateIoCompletionPort((HANDLE)desc->udp_socket_ref, iocp,
                                               (ULONG_PTR)desc->port, 0);
    if (!associated) {
        KRS_LOG_ERROR("iocp", "CreateIoCompletionPort association failed for port %u (err=%lu)",
                      desc->port, GetLastError());
        return;
    }

    int rc = WSARecvFrom(desc->udp_socket_ref, &ctx->wsabuf, 1, NULL, &ctx->flags,
                         (struct sockaddr*)&ctx->remote_addr, &ctx->remote_addr_len,
                         &ctx->overlapped, NULL);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        KRS_LOG_ERROR("iocp", "initial WSARecvFrom failed for port %u (err=%d)",
                      desc->port, WSAGetLastError());
    }
}

Void_r krs_server_start(ServerPortManager_t* spm) {
    Void_r result = {0};

    bool wsa_init          = false;
    bool iocp_created      = false;
    bool queue_created     = false;
    bool pool_created      = false;
    bool map_created       = false;
    bool running_set       = false;
    uint32_t iocp_started  = 0;
    uint32_t handler_started = 0;

    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }
    if (spm->running) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_ALREADY_RUNNING, "already running");
        return result;
    }

    krs_wsa_init();
    wsa_init = true;

    spm->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (spm->iocp_handle == NULL) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET, "CreateIoCompletionPort failed");
        goto fail;
    }
    iocp_created = true;

    spm->message_queue = krs_message_queue_create(256);
    if (!spm->message_queue) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "message queue alloc failed");
        goto fail;
    }
    queue_created = true;

    spm->message_pool = krs_message_pool_create(1024);
    if (!spm->message_pool) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "message pool alloc failed");
        goto fail;
    }
    pool_created = true;

    krs_message_queue_set_pool(spm->message_queue, spm->message_pool);

    spm->connection_map = krs_connection_map_create(64);
    if (!spm->connection_map) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "connection map alloc failed");
        goto fail;
    }
    map_created = true;

    if (spm->descriptor_list) {
        uint32_t desc_count = krs_array_length(spm->descriptor_list);
        for (uint32_t i = 0; i < desc_count; i++) {
            UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, i, UDPSocketDescriptor_t);
            if (!desc) continue;
            s_post_initial_recv(spm->iocp_handle, desc);
        }
    }

    spm->running = true;
    running_set = true;

    spm->iocp_threads = malloc(spm->config_iocp_threads * sizeof(HANDLE));
    if (!spm->iocp_threads) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "thread array alloc failed");
        goto fail;
    }
    spm->iocp_thread_count = spm->config_iocp_threads;

    for (uint32_t i = 0; i < spm->config_iocp_threads; i++) {
        spm->iocp_threads[i] = CreateThread(NULL, 0, krs_server_iocp_io_thread, spm, 0, NULL);
        if (!spm->iocp_threads[i]) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET,
                                                           "CreateThread failed for IOCP thread");
            goto fail;
        }
        iocp_started = i + 1;
    }

    spm->handler_threads = malloc(spm->config_handler_threads * sizeof(HANDLE));
    if (!spm->handler_threads) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "handler thread array alloc failed");
        goto fail;
    }
    spm->handler_thread_count = spm->config_handler_threads;

    for (uint32_t i = 0; i < spm->config_handler_threads; i++) {
        spm->handler_threads[i] = CreateThread(NULL, 0, krs_server_message_handler_thread, spm, 0, NULL);
        if (!spm->handler_threads[i]) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET,
                                                           "CreateThread failed for handler thread");
            goto fail;
        }
        handler_started = i + 1;
    }

    spm->retransmit_thread = CreateThread(NULL, 0, krs_server_retransmit_thread, spm, 0, NULL);

    result.base = krs_lib_error_result_base_suc();
    return result;

fail:
    if (running_set) {
        spm->running = false;
    }

    if (handler_started > 0) {
        if (spm->message_queue) {
            krs_message_queue_stop(spm->message_queue);
        }
        WaitForMultipleObjects(handler_started, spm->handler_threads, TRUE, 2000);
        for (uint32_t j = 0; j < handler_started; j++) {
            CloseHandle(spm->handler_threads[j]);
        }
    }
    if (spm->handler_threads) {
        free(spm->handler_threads);
        spm->handler_threads = NULL;
        spm->handler_thread_count = 0;
    }

    if (iocp_started > 0) {
        for (uint32_t j = 0; j < iocp_started; j++) {
            PostQueuedCompletionStatus(spm->iocp_handle, 0, SHUTDOWN_KEY, NULL);
        }
        WaitForMultipleObjects(iocp_started, spm->iocp_threads, TRUE, 2000);
        for (uint32_t j = 0; j < iocp_started; j++) {
            CloseHandle(spm->iocp_threads[j]);
        }
    }
    if (spm->iocp_threads) {
        free(spm->iocp_threads);
        spm->iocp_threads = NULL;
        spm->iocp_thread_count = 0;
    }

    if (map_created) {
        krs_connection_map_destroy(&spm->connection_map);
    }
    if (pool_created) {
        krs_message_pool_destroy(&spm->message_pool);
    }
    if (queue_created) {
        krs_message_queue_destroy(&spm->message_queue);
    }
    if (iocp_created) {
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
    }
    if (wsa_init) {
        krs_wsa_cleanup();
    }

    return result;
}

void krs_server_stop(ServerPortManager_t* spm) {
    if (!spm || !spm->running) return;

    spm->running = false;

    for (uint32_t i = 0; i < spm->iocp_thread_count; i++) {
        PostQueuedCompletionStatus(spm->iocp_handle, 0, SHUTDOWN_KEY, NULL);
    }

    if (spm->iocp_threads) {
        WaitForMultipleObjects(spm->iocp_thread_count, spm->iocp_threads, TRUE, 5000);
        for (uint32_t i = 0; i < spm->iocp_thread_count; i++) {
            CloseHandle(spm->iocp_threads[i]);
        }
        free(spm->iocp_threads);
        spm->iocp_threads = NULL;
        spm->iocp_thread_count = 0;
    }

    if (spm->message_queue) {
        krs_message_queue_stop(spm->message_queue);
    }

    if (spm->handler_threads) {
        WaitForMultipleObjects(spm->handler_thread_count, spm->handler_threads, TRUE, 5000);
        for (uint32_t i = 0; i < spm->handler_thread_count; i++) {
            CloseHandle(spm->handler_threads[i]);
        }
        free(spm->handler_threads);
        spm->handler_threads = NULL;
        spm->handler_thread_count = 0;
    }

    if (spm->retransmit_thread) {
        WaitForSingleObject(spm->retransmit_thread, 2000);
        CloseHandle(spm->retransmit_thread);
        spm->retransmit_thread = NULL;
    }

    if (spm->descriptor_list) {
        uint32_t count = krs_array_length(spm->descriptor_list);
        for (uint32_t i = 0; i < count; i++) {
            UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, i, UDPSocketDescriptor_t);
            if (desc && desc->recv_ctx) {
                free(desc->recv_ctx);
                desc->recv_ctx = NULL;
            }
        }
    }

    if (spm->iocp_handle) {
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
    }

    krs_message_queue_drain(spm->message_queue, spm->message_pool);
    krs_message_queue_destroy(&spm->message_queue);
    krs_message_pool_destroy(&spm->message_pool);
    krs_connection_map_destroy(&spm->connection_map);
    krs_wsa_cleanup();
}
