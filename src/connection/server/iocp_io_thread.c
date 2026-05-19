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

DWORD WINAPI krs_server_iocp_io_thread(LPVOID param) {
    ServerPortManager_t* spm = (ServerPortManager_t*)param;

    while (spm->running) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped;

        BOOL ok = GetQueuedCompletionStatus(spm->iocp_handle, &bytes_transferred,
                                            &completion_key, &overlapped, INFINITE);

        if (completion_key == SHUTDOWN_KEY) break;
        if (!spm->running) break;
        if (!overlapped) continue;

        UDPOverlapped_t* ctx = (UDPOverlapped_t*)overlapped;

        if (ok && bytes_transferred > 0) {
            IncomingMessage_t* msg = krs_message_pool_acquire(spm->message_pool);
            if (msg) {
                memcpy(msg->data, ctx->buffer, bytes_transferred);
                msg->data_length = bytes_transferred;
                msg->remote_address = ctx->remote_addr;
                msg->port = ctx->port;
                krs_message_queue_push(spm->message_queue, msg);
            } else {
                KRS_LOG_ERROR("iocp", "message pool acquire failed, dropping %lu bytes",
                              (unsigned long)bytes_transferred);
            }
        }

        memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
        ctx->remote_addr_len = sizeof(ctx->remote_addr);
        ctx->flags = 0;
        ctx->wsabuf.buf = (char*)ctx->buffer;
        ctx->wsabuf.len = sizeof(ctx->buffer);

        int rc = WSARecvFrom(ctx->socket, &ctx->wsabuf, 1, NULL, &ctx->flags,
                             (struct sockaddr*)&ctx->remote_addr, &ctx->remote_addr_len,
                             &ctx->overlapped, NULL);
        if (rc == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                KRS_LOG_WARN("iocp", "WSARecvFrom re-arm failed (err=%d), retrying once", err);
                Sleep(1);
                memset(&ctx->overlapped, 0, sizeof(ctx->overlapped));
                ctx->remote_addr_len = sizeof(ctx->remote_addr);
                ctx->flags = 0;
                int rc2 = WSARecvFrom(ctx->socket, &ctx->wsabuf, 1, NULL, &ctx->flags,
                                      (struct sockaddr*)&ctx->remote_addr, &ctx->remote_addr_len,
                                      &ctx->overlapped, NULL);
                if (rc2 == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                    KRS_LOG_ERROR("iocp", "WSARecvFrom retry also failed (err=%d), socket dead",
                                  WSAGetLastError());
                }
            }
        }
    }

    return 0;
}
