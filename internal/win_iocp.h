#ifndef WIN_IOCP_H
#define WIN_IOCP_H

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_BUF_LEN 1024 //TODO: add this to config

typedef struct IOSocketData IOSocketData_t;

struct IOSocketData {
    OVERLAPPED overlapped; // Required as first member
    WSABUF buf_data;
    uint8_t buffer[MAX_BUF_LEN];
    struct sockaddr_in client_addr;
    int client_addr_len;
    DWORD flags;
};




#endif //WIN_IOCP_H
