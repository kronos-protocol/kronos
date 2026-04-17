#include "kronos_client.h"
#include "kronos_network.h"
#include "kronos_log.h"
#include "client_internal.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define DEMO_PORT 9001
#define DEMO_CHANNEL 10

static void s_log_callback(KronosLogLevel_e level, const char* component, const char* message) {
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    printf("[%s] %s: %s\n", level_str[level], component, message);
}

static volatile int s_received_count = 0;

static void s_on_server_message(Channel_t channel, ChannelType_e channel_type,
                                uint32_t connection_id, const uint8_t* data,
                                uint16_t data_length, void* user_data) {
    (void)channel_type;
    (void)connection_id;
    (void)user_data;

    printf("[CLIENT] Echo from server on ch %u (%u bytes): ", channel, data_length);

    for (uint16_t i = 0; i < data_length && i < 64; i++) {
        if (data[i] >= 0x20 && data[i] < 0x7F) {
            putchar(data[i]);
        } else {
            printf("\\x%02X", data[i]);
        }
    }
    if (data_length > 64) printf("...");
    printf("\n");

    s_received_count++;
}

int main(void) {
    printf("=== Kronos Demo Client ===\n");
    printf("Connecting to 127.0.0.1:%d...\n\n", DEMO_PORT);

    krs_log_callback_set(s_log_callback);

    Address_t addr = krs_network_address_ipv4_create("127.0.0.1");
    PortAddress_t server_addr = krs_network_port_address_create(DEMO_PORT, addr);

    ServerConnection_t* conn = krs_client_server_connect(server_addr);
    if (!conn) {
        printf("Connection failed. Is the server running?\n");
        return 1;
    }

    printf("Connected! Connection ID: %u\n\n", conn->connection_id);

    krs_client_set_callback(conn, s_on_server_message, NULL);
    Void_r recv_r = krs_client_start_receive(conn);
    if (!recv_r.base.valid) {
        printf("Failed to start receive: %s\n",
               recv_r.base.error_message ? recv_r.base.error_message : "unknown");
        krs_client_disconnect(&conn);
        return 1;
    }

    const char* messages[] = {
        "Hello from Kronos!",
        "This is message number two.",
        "UDP with reliable delivery.",
    };
    int message_count = sizeof(messages) / sizeof(messages[0]);

    for (int i = 0; i < message_count; i++) {
        uint16_t len = (uint16_t)strlen(messages[i]);
        bool require_ack = (i == message_count - 1);

        printf("[CLIENT] Sending: \"%s\"%s\n", messages[i],
               require_ack ? " (with ACK)" : "");

        Void_r send_r = krs_client_send(conn, DEMO_CHANNEL,
                                        (const uint8_t*)messages[i], len, require_ack);
        if (!send_r.base.valid) {
            printf("[CLIENT] Send failed: %s\n",
                   send_r.base.error_message ? send_r.base.error_message : "unknown");
        }

        Sleep(200);
    }

    printf("\n[CLIENT] Sending binary payload (16 bytes)...\n");
    uint8_t binary_data[16];
    for (int i = 0; i < 16; i++) binary_data[i] = (uint8_t)(i * 17);
    krs_client_send(conn, DEMO_CHANNEL, binary_data, sizeof(binary_data), false);

    Sleep(500);

    printf("\n[CLIENT] Received %d echo(es) from server.\n", s_received_count);
    printf("[CLIENT] Disconnecting...\n");
    krs_client_disconnect(&conn);
    printf("[CLIENT] Done.\n");

    return 0;
}
