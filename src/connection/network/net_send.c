#include "kronos_network.h"

#include "net_send_internal.h"
#include "kronos_log.h"

#include <winsock2.h>


void krs_net_send_frame(UDPSocketRef_t socket, const PortAddress_t* addr,
                        const uint8_t* frame_data, uint16_t frame_size) {
    if (frame_size == 0 || !frame_data || !addr) return;

    WSABUF wsabuf;
    wsabuf.len = frame_size;
    wsabuf.buf = (char*)frame_data;
    DWORD sent;
    int rc = WSASendTo(socket, &wsabuf, 1, &sent, 0,
                       (const struct sockaddr*)addr, sizeof(*addr), NULL, NULL);
    if (rc == SOCKET_ERROR) {
        KRS_LOG_WARN("net_send", "WSASendTo failed (err=%d, size=%u)",
                     WSAGetLastError(), frame_size);
    }
}
