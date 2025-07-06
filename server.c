#include "include/kronos.h"

#include <stdint.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
        printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sockfd == INVALID_SOCKET ) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9001);

    if ( bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR ) {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    printf("UDP Server running on port 8080...\n");

    uint8_t buffer[1024];
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    while ( 1 ) {
        uint16_t received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &client_len); // This leaves one space open for Null Termination of string

        if ( received == SOCKET_ERROR ) {
            printf("recvfrom failed: %d\n", WSAGetLastError());
            continue;
        }

        buffer[received] = '\0';
        printf("Received: %s\n", buffer);
        uint8_t frame_data[krs_frame_calculate_body_length(received)];
        Frame_t frame = krs_frame_create(buffer, received, frame_data, received);
        uint8_t out[received];
        uint16_t body_size = krs_frame_get_content(&frame, out, received);
        printf("Frame created\n");

        sendto(sockfd, out, body_size, 0, (struct sockaddr*)&client_addr, client_len);
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}