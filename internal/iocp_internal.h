#ifndef IOCP_INTERNAL_H
#define IOCP_INTERNAL_H

#include "kronos_network.h"
#include "kronos_internal.h"

#include <winsock2.h>
#include <mswsock.h>

typedef struct UDPOverlapped UDPOverlapped_t;

struct UDPOverlapped {
    WSAOVERLAPPED overlapped;
    WSABUF        wsabuf;
    uint8_t       buffer[KRONOS_BUFFER_SIZE];
    struct sockaddr_in6 remote_addr;
    INT           remote_addr_len;
    DWORD         flags;
    Port_t        port;
    UDPSocketRef_t socket;
};

DWORD WINAPI krs_server_iocp_io_thread(LPVOID param);
DWORD WINAPI krs_server_message_handler_thread(LPVOID param);

#endif // IOCP_INTERNAL_H
