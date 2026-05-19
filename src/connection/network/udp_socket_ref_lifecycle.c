#include <kronos_network.h>
#include <kronos_log.h>
#include <network_internal.h>

#include <mswsock.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

UDPSocketRef_t krs_network_udp_socket_ref_create(PortAddress_t port_address) {
    struct sockaddr_in6 addr = port_address;
    if (addr.sin6_family != AF_INET6) {
        KRS_LOG_ERROR("udp_socket", "invalid address family %d (expected AF_INET6)",
                      addr.sin6_family);
        return INVALID_SOCKET;
    }

    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        KRS_LOG_ERROR("udp_socket", "socket() failed (err=%d)", WSAGetLastError());
        return INVALID_SOCKET;
    }

    DWORD off = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

    BOOL connreset_disabled = FALSE;
    DWORD bytes_returned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &connreset_disabled, sizeof(connreset_disabled),
             NULL, 0, &bytes_returned, NULL, NULL);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        KRS_LOG_ERROR("udp_socket", "bind() failed (err=%d)", WSAGetLastError());
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}

