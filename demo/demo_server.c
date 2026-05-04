#include "kronos_server.h"
#include "kronos_network.h"
#include "kronos_log.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define DEMO_PORT 9001

static ServerPortManager_t* s_spm = NULL;

static void s_log_callback(KronosLogLevel_e level, const char* component, const char* message) {
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    printf("[%s] %s: %s\n", level_str[level], component, message);
}

static void s_on_connect(uint32_t connection_id, Channel_t channel, void* user_data) {
    (void)user_data;
    printf("[SERVER] Client connected: id=%u channel=%u\n", connection_id, channel);
}

static void s_on_disconnect(uint32_t connection_id, Channel_t channel, void* user_data) {
    (void)user_data;
    printf("[SERVER] Client disconnected: id=%u channel=%u\n", connection_id, channel);
}

static void s_on_message(Channel_t channel, ChannelType_e channel_type,
                         uint32_t connection_id, const uint8_t* data,
                         uint16_t data_length, void* user_data) {
    (void)channel_type;
    (void)user_data;

    printf("[SERVER] Message from client %u on ch %u (%u bytes): ",
           connection_id, channel, data_length);

    for (uint16_t i = 0; i < data_length && i < 64; i++) {
        if (data[i] >= 0x20 && data[i] < 0x7F) {
            putchar(data[i]);
        } else {
            printf("\\x%02X", data[i]);
        }
    }
    if (data_length > 64) printf("...");
    printf("\n");

    if (s_spm && connection_id > 0) {
        Void_r result = krs_server_send(s_spm, connection_id, channel,
                                        data, data_length, false);
        if (result.base.valid) {
            printf("[SERVER] Echoed %u bytes back to client %u\n", data_length, connection_id);
        } else {
            printf("[SERVER] Echo failed: %s\n",
                   result.base.error_message ? result.base.error_message : "unknown");
        }
    }
}

int main(void) {
    printf("=== Kronos Demo Server ===\n");
    printf("Starting on port %d...\n\n", DEMO_PORT);

    krs_log_callback_set(s_log_callback);

    Address_t addr = krs_network_address_ipv4_create("0.0.0.0");
    s_spm = krs_server_port_manager_create(addr);
    if (!s_spm) {
        printf("Failed to create server port manager\n");
        return 1;
    }

    Void_r port_r = krs_server_port_manager_port_add(s_spm, DEMO_PORT);
    if (!port_r.base.valid) {
        printf("Failed to add port %d: %s\n", DEMO_PORT,
               port_r.base.error_message ? port_r.base.error_message : "unknown");
        krs_server_port_manager_destroy(&s_spm);
        return 1;
    }
    krs_server_set_port_callback(s_spm, DEMO_PORT, s_on_message, NULL);
    krs_server_set_connect_callback(s_spm, DEMO_PORT, s_on_connect, NULL);
    krs_server_set_disconnect_callback(s_spm, DEMO_PORT, s_on_disconnect, NULL);

    Void_r start_r = krs_server_start(s_spm);
    if (!start_r.base.valid) {
        printf("Failed to start server: %s\n",
               start_r.base.error_message ? start_r.base.error_message : "unknown");
        krs_server_port_manager_destroy(&s_spm);
        return 1;
    }

    printf("Server running. Press Enter to stop.\n\n");
    getchar();

    printf("\nStopping server...\n");
    krs_server_stop(s_spm);
    krs_server_port_manager_destroy(&s_spm);
    printf("Server stopped.\n");

    return 0;
}
