#ifndef KRONOS_PLATFORM_H
#define KRONOS_PLATFORM_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

/** @brief Platform socket handle. On Windows: SOCKET. On Linux: int (future). */
typedef SOCKET KrsSocketHandle_t;

/** @brief Platform network address. On Windows: sockaddr_in6. On Linux: sockaddr_in6 (future). */
typedef struct sockaddr_in6 KrsNetworkAddress_t;

/** @brief Invalid socket sentinel value. */
#define KRS_INVALID_SOCKET INVALID_SOCKET

#else
#error "Only Windows is currently supported"
#endif

#endif // KRONOS_PLATFORM_H
