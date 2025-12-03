#include <kronos_server.h>
#include <server_internal.h>


UDPSocketRef_t krs_network_udp_socket_ref_create(PortAddress_t port_address) {
    struct sockaddr_in6 addr = port_address;
    if (addr.sin6_family != AF_INET6) return INVALID_SOCKET;

    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    DWORD off = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

    if (bind(s, addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}
