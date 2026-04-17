#include "kronos_server.h"

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
    if (!ctx) return;

    ctx->port = desc->port;
    ctx->socket = desc->udp_socket_ref;
    ctx->wsabuf.buf = (char*)ctx->buffer;
    ctx->wsabuf.len = sizeof(ctx->buffer);
    ctx->remote_addr_len = sizeof(ctx->remote_addr);
    ctx->flags = 0;

    desc->recv_ctx = ctx;

    CreateIoCompletionPort((HANDLE)desc->udp_socket_ref, iocp, (ULONG_PTR)desc->port, 0);

    WSARecvFrom(desc->udp_socket_ref, &ctx->wsabuf, 1, NULL, &ctx->flags,
                (struct sockaddr*)&ctx->remote_addr, &ctx->remote_addr_len,
                &ctx->overlapped, NULL);
}

Void_r krs_server_start(ServerPortManager_t* spm) {
    Void_r result = {0};

    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }
    if (spm->running) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_ALREADY_RUNNING, "already running");
        return result;
    }

    krs_wsa_init();

    spm->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (spm->iocp_handle == NULL) {
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET, "CreateIoCompletionPort failed");
        return result;
    }

    spm->message_queue = krs_message_queue_create(256);
    if (!spm->message_queue) {
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "message queue alloc failed");
        return result;
    }

    spm->message_pool = krs_message_pool_create(1024);
    if (!spm->message_pool) {
        krs_message_queue_destroy(&spm->message_queue);
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "message pool alloc failed");
        return result;
    }

    krs_message_queue_set_pool(spm->message_queue, spm->message_pool);

    spm->connection_map = krs_connection_map_create(64);
    if (!spm->connection_map) {
        krs_message_pool_destroy(&spm->message_pool);
        krs_message_queue_destroy(&spm->message_queue);
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "connection map alloc failed");
        return result;
    }

    if (spm->descriptor_list) {
        uint32_t desc_count = krs_array_length(spm->descriptor_list);
        for (uint32_t i = 0; i < desc_count; i++) {
            UDPSocketDescriptor_t* desc = KRS_ARRAY_GET(spm->descriptor_list, i, UDPSocketDescriptor_t);
            if (!desc) continue;
            s_post_initial_recv(spm->iocp_handle, desc);
        }
    }

    spm->running = true;

    spm->iocp_threads = malloc(spm->config_iocp_threads * sizeof(HANDLE));
    if (!spm->iocp_threads) {
        spm->running = false;
        krs_message_queue_destroy(&spm->message_queue);
        krs_message_pool_destroy(&spm->message_pool);
        krs_connection_map_destroy(&spm->connection_map);
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "thread array alloc failed");
        return result;
    }
    spm->iocp_thread_count = spm->config_iocp_threads;
    for (uint32_t i = 0; i < spm->config_iocp_threads; i++) {
        spm->iocp_threads[i] = CreateThread(NULL, 0, krs_server_iocp_io_thread, spm, 0, NULL);
        if (!spm->iocp_threads[i]) {
            spm->running = false;
            for (uint32_t j = 0; j < i; j++) {
                PostQueuedCompletionStatus(spm->iocp_handle, 0, SHUTDOWN_KEY, NULL);
            }
            for (uint32_t j = 0; j < i; j++) {
                WaitForSingleObject(spm->iocp_threads[j], 2000);
                CloseHandle(spm->iocp_threads[j]);
            }
            free(spm->iocp_threads);
            spm->iocp_threads = NULL;
            spm->iocp_thread_count = 0;
            krs_message_queue_destroy(&spm->message_queue);
            krs_message_pool_destroy(&spm->message_pool);
            krs_connection_map_destroy(&spm->connection_map);
            CloseHandle(spm->iocp_handle);
            spm->iocp_handle = NULL;
            krs_wsa_cleanup();
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET,
                                                           "CreateThread failed for IOCP thread");
            return result;
        }
    }

    spm->handler_threads = malloc(spm->config_handler_threads * sizeof(HANDLE));
    if (!spm->handler_threads) {
        spm->running = false;
        for (uint32_t i = 0; i < spm->iocp_thread_count; i++) {
            PostQueuedCompletionStatus(spm->iocp_handle, 0, SHUTDOWN_KEY, NULL);
        }
        WaitForMultipleObjects(spm->iocp_thread_count, spm->iocp_threads, TRUE, 2000);
        for (uint32_t i = 0; i < spm->iocp_thread_count; i++) {
            CloseHandle(spm->iocp_threads[i]);
        }
        free(spm->iocp_threads);
        spm->iocp_threads = NULL;
        spm->iocp_thread_count = 0;
        krs_message_queue_destroy(&spm->message_queue);
        krs_message_pool_destroy(&spm->message_pool);
        krs_connection_map_destroy(&spm->connection_map);
        CloseHandle(spm->iocp_handle);
        spm->iocp_handle = NULL;
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "handler thread array alloc failed");
        return result;
    }
    spm->handler_thread_count = spm->config_handler_threads;
    for (uint32_t i = 0; i < spm->config_handler_threads; i++) {
        spm->handler_threads[i] = CreateThread(NULL, 0, krs_server_message_handler_thread, spm, 0, NULL);
        if (!spm->handler_threads[i]) {
            spm->running = false;
            if (spm->message_queue) {
                krs_message_queue_stop(spm->message_queue);
            }
            for (uint32_t j = 0; j < i; j++) {
                WaitForSingleObject(spm->handler_threads[j], 2000);
                CloseHandle(spm->handler_threads[j]);
            }
            free(spm->handler_threads);
            spm->handler_threads = NULL;
            spm->handler_thread_count = 0;
            for (uint32_t k = 0; k < spm->iocp_thread_count; k++) {
                PostQueuedCompletionStatus(spm->iocp_handle, 0, SHUTDOWN_KEY, NULL);
            }
            WaitForMultipleObjects(spm->iocp_thread_count, spm->iocp_threads, TRUE, 2000);
            for (uint32_t k = 0; k < spm->iocp_thread_count; k++) {
                CloseHandle(spm->iocp_threads[k]);
            }
            free(spm->iocp_threads);
            spm->iocp_threads = NULL;
            spm->iocp_thread_count = 0;
            krs_message_queue_destroy(&spm->message_queue);
            krs_message_pool_destroy(&spm->message_pool);
            krs_connection_map_destroy(&spm->connection_map);
            CloseHandle(spm->iocp_handle);
            spm->iocp_handle = NULL;
            krs_wsa_cleanup();
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_PLATFORM_WINDOWS_SOCKET,
                                                           "CreateThread failed for handler thread");
            return result;
        }
    }

    spm->retransmit_thread = CreateThread(NULL, 0, krs_server_retransmit_thread, spm, 0, NULL);

    result.base = krs_lib_error_result_base_suc();
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
