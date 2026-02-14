#ifndef KRONOS_NETWORK_H
#define KRONOS_NETWORK_H
#include "kronos_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")  // Link Winsock lib in MSVC
    typedef SOCKET UDPSocketRef_t;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <errno.h>
    typedef int UDPSocketRef_t;
#endif

typedef struct Endpoint Endpoint_t;
typedef uint8_t Channel_t;
typedef struct Connection Connection_t;
typedef struct sockaddr_in6 PortAddress_t;
typedef struct in6_addr Address_t;
typedef struct AddressCreateResult AddressCreate_r;
typedef uint16_t Port_t;
typedef struct UDPSocketRefPort UDPSocketRefPort_t;
typedef struct UDPThreadedSocketHandler UDPThreadedSocketHandler_t;

typedef enum ChannelType ChannelType_e;

struct UDPSocketRefPort {
    UDPSocketRef_t socket;
    int port;
};

struct AddressCreateResult {
    KronosResult_b base;
    Address_t address;
};

enum ChannelType {
    OPEN_CHANNEL,
    MESSAGE_CHANNEL,
    SOCKET_CHANNEL
};

Address_t krs_network_address_ipv4_create(const char* ip);
AddressCreate_r krs_network_address_ipv4_create_s(const char* ip);
Address_t krs_network_address_ipv4_create_len(const char* ip, const uint8_t ip_length);
AddressCreate_r krs_network_address_ipv4_create_len_s(const char* ip, const uint8_t ip_length);

Address_t krs_network_address_ipv6_create(char* ip, uint8_t ip_length);

PortAddress_t krs_network_port_address_create(Port_t port, Address_t address);

UDPSocketRef_t krs_network_udp_socket_ref_create(PortAddress_t port_address);

UDPThreadedSocketHandler_t* krs_network_udp_socket_handler_threaded_create(int thread_count);
Void_r krs_network_udp_socket_handler_threaded_handle_add(UDPThreadedSocketHandler_t* udp_tsh, UDPSocketRef_t* udp_sr);
void krs_network_udp_socket_handler_threaded_destroy(UDPThreadedSocketHandler_t** udp_tsh, bool destroy_socket_refs);

#define MAX_CHANNEL_NUMBER 255
#define MAX_PORT_NUMBER 65535

#define MAX_IPV6_LENGTH 46
#define MAX_IPV4_LENGTH 16

#endif //KRONOS_NETWORK_H
