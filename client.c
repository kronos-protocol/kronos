#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9001);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    char message[1024];
    char buffer[1024];

    while (1) {
        printf("Message: ");
        fgets(message, sizeof(message), stdin);

        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "quit") == 0) break;

        sendto(sockfd, message, strlen(message), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));

        int received = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, NULL, NULL);
        if (received > 0) {
            buffer[received] = '\0';
            printf("Server: %s\n", buffer);
        }
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}
