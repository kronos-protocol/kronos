#include "kronos_error.h"

const char* krs_error_get_message(krs_error_t error) {
    switch (error) {
        case KRS_SUCCESS:                        return "Success";
        case KRS_ERR_NULL_POINTER:               return "Null pointer argument";
        case KRS_ERR_INVALID_PARAMETER:          return "Invalid parameter";
        case KRS_ERR_MEMORY_ALLOCATION:          return "Memory allocation failed";
        case KRS_ERR_BUFFER_TOO_SMALL:           return "Buffer too small";
        case KRS_ERR_NOT_INITIALIZED:            return "Not initialized";
        case KRS_ERR_ALREADY_INITIALIZED:        return "Already initialized";
        case KRS_ERR_NOT_NULL_TERMINATED:        return "String not null-terminated within buffer";
        case KRS_ERR_TOO_LONG:                   return "Input exceeds maximum length";
        case KRS_ERR_FRAME_INVALID_HEADER:       return "Frame header is invalid or too short";
        case KRS_ERR_FRAME_INVALID_PROTOCOL:     return "Frame does not start with magic byte 0x4B";
        case KRS_ERR_FRAME_CORRUPT_DATA:         return "Frame body data is corrupt";
        case KRS_ERR_FRAME_UNSUPPORTED_VERSION:  return "Frame protocol version is not supported";
        case KRS_ERR_FRAME_INVALID_TYPE:         return "Unknown frame type";
        case KRS_ERR_FRAME_BODY_TOO_LARGE:       return "Frame body exceeds maximum size";
        case KRS_ERR_FRAME_ALREADY_FREED:        return "Frame has already been freed";
        case KRS_ERR_NETWORK_INVALID_IP:         return "Invalid IP address format";
        case KRS_ERR_NETWORK_INVALID_PORT:       return "Invalid port number";
        case KRS_ERR_NETWORK_CONNECTION_FAILED:  return "Network connection failed";
        case KRS_ERR_NETWORK_SOCKET_ERROR:       return "Socket error";
        case KRS_ERR_NETWORK_TIMEOUT:            return "Network operation timed out";
        case KRS_ERR_NETWORK_ADDRESS_IN_USE:     return "Address already in use";
        case KRS_ERR_NETWORK_UNREACHABLE:        return "Network unreachable";
        case KRS_ERR_SERVER_PORT_IN_USE:         return "Port is already in use by the server";
        case KRS_ERR_SERVER_MAX_CONNECTIONS:     return "Maximum connections reached";
        case KRS_ERR_SERVER_NOT_LISTENING:       return "Server is not listening";
        case KRS_ERR_SERVER_BIND_FAILED:         return "Server socket bind failed";
        case KRS_ERR_SERVER_ALREADY_RUNNING:     return "Server is already running";
        case KRS_ERR_SERVER_CONGESTION_WINDOW_FULL: return "congestion window full";
        case KRS_ERR_CLIENT_NOT_CONNECTED:       return "Client is not connected";
        case KRS_ERR_CLIENT_CONNECTION_LOST:     return "Client connection was lost";
        case KRS_ERR_CLIENT_HANDSHAKE_FAILED:    return "Client handshake failed";
        case KRS_ERR_CLIENT_AUTH_FAILED:         return "Client authentication failed";
        case KRS_ERR_CLIENT_TIMEOUT:             return "Client operation timed out";
        case KRS_ERR_MATH_OVERFLOW:              return "Arithmetic overflow";
        case KRS_ERR_MATH_UNDERFLOW:             return "Arithmetic underflow";
        case KRS_ERR_MATH_INVALID_BITMASK:       return "Invalid bitmask";
        case KRS_ERR_MATH_DIVISION_BY_ZERO:      return "Division by zero";
        case KRS_ERR_PLATFORM_WINDOWS_SOCKET:    return "Windows socket error";
        case KRS_ERR_PLATFORM_LINUX_EPOLL:       return "Linux epoll error";
        case KRS_ERR_PLATFORM_UNSUPPORTED:       return "Platform not supported";
        default:                                 return "Unknown error";
    }
}

KronosErrorCategory_e krs_error_get_category(krs_error_t error) {
    if (error == KRS_SUCCESS)           return KRS_ERR_CATEGORY_SUCCESS;
    if (error < 100)                    return KRS_ERR_CATEGORY_GENERAL;
    if (error < 200)                    return KRS_ERR_CATEGORY_FRAME;
    if (error < 300)                    return KRS_ERR_CATEGORY_NETWORK;
    if (error < 400)                    return KRS_ERR_CATEGORY_SERVER;
    if (error < 500)                    return KRS_ERR_CATEGORY_CLIENT;
    if (error < 600)                    return KRS_ERR_CATEGORY_MATH;
    return KRS_ERR_CATEGORY_PLATFORM;
}

bool krs_error_is_fatal(krs_error_t error) {
    switch (error) {
        case KRS_ERR_MEMORY_ALLOCATION:
        case KRS_ERR_FRAME_CORRUPT_DATA:
        case KRS_ERR_NETWORK_SOCKET_ERROR:
        case KRS_ERR_SERVER_BIND_FAILED:
        case KRS_ERR_PLATFORM_WINDOWS_SOCKET:
        case KRS_ERR_PLATFORM_UNSUPPORTED:
            return true;
        default:
            return false;
    }
}
