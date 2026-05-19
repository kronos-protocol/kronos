#include "kronos_server.h"
#include "kronos_stats.h"
#include "kronos_log.h"

#include "server_internal.h"
#include "message_pool_internal.h"

#include <stdlib.h>

#include <winsock2.h>


ServerPortManager_t* krs_server_port_manager_create(Address_t default_address) {
    ServerPortManager_t* server_port_manager = calloc(1, sizeof(ServerPortManager_t));
    if (!server_port_manager) return NULL;

    PortTable_t* port_table = krs_lib_port_table_create();
    if (!port_table) {
        free(server_port_manager);
        return NULL;
    }

    KrsArray_t* descriptor_list = krs_array_create(8);
    if (!descriptor_list) {
        krs_lib_port_table_destroy(&port_table);
        free(server_port_manager);
        return NULL;
    }

    server_port_manager->port_table = port_table;
    server_port_manager->descriptor_list = descriptor_list;
    server_port_manager->default_address = default_address;
    server_port_manager->default_max_channel = 255;
    server_port_manager->config_iocp_threads = 2;
    server_port_manager->config_handler_threads = 4;
    return server_port_manager;
}

Void_r krs_server_port_manager_port_add(ServerPortManager_t* spm, Port_t port) {
    Void_r result = {0};

    if (!krs_server_port_manager_validate(spm)) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "invalid spm");
        return result;
    }

    krs_wsa_init();
    PortAddress_t port_address = krs_network_port_address_create(port, spm->default_address);
    UDPSocketDescriptor_t* socket_handler = krs_server_udp_socket_handler_create(port_address);
    if (!socket_handler) {
        KRS_LOG_ERROR("port_manager", "port_add(%u) failed: socket creation failed", port);
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_BIND_FAILED, "socket creation failed");
        return result;
    }

    socket_handler->port = port;
    krs_lib_port_table_insert(spm->port_table, port, socket_handler);

    Void_r push_r = krs_array_push(spm->descriptor_list, socket_handler);
    if (!push_r.base.valid) {
        KRS_LOG_ERROR("port_manager", "port_add(%u) failed: descriptor list push failed", port);
        krs_server_udp_socket_handler_destroy(&socket_handler);
        krs_wsa_cleanup();
        return push_r;
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_port_manager_port_add_with_address(ServerPortManager_t* spm, Port_t port, Address_t address) {
    Void_r result = {0};

    if (!krs_server_port_manager_validate(spm)) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "invalid spm");
        return result;
    }

    krs_wsa_init();
    PortAddress_t port_address = krs_network_port_address_create(port, address);
    UDPSocketDescriptor_t* socket_handler = krs_server_udp_socket_handler_create(port_address);
    if (!socket_handler) {
        KRS_LOG_ERROR("port_manager", "port_add_with_address(%u) failed: socket creation failed", port);
        krs_wsa_cleanup();
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_SERVER_BIND_FAILED, "socket creation failed");
        return result;
    }

    socket_handler->port = port;
    krs_lib_port_table_insert(spm->port_table, port, socket_handler);

    Void_r push_r = krs_array_push(spm->descriptor_list, socket_handler);
    if (!push_r.base.valid) {
        KRS_LOG_ERROR("port_manager", "port_add_with_address(%u) failed: descriptor list push failed", port);
        krs_server_udp_socket_handler_destroy(&socket_handler);
        krs_wsa_cleanup();
        return push_r;
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
}

void krs_server_port_manager_destroy(ServerPortManager_t** spm) {
    if (!spm || !*spm) return;
    ServerPortManager_t* m = *spm;
    uint32_t port_count = m->descriptor_list ? krs_array_length(m->descriptor_list) : 0;
    krs_array_destroy(&m->descriptor_list);
    krs_lib_port_table_destroy(&m->port_table);
    free(m);
    *spm = NULL;
    for (uint32_t i = 0; i < port_count; i++) {
        krs_wsa_cleanup();
    }
}

void krs_server_set_thread_counts(ServerPortManager_t* spm,
                                  uint32_t iocp_threads, uint32_t handler_threads) {
    if (!spm) return;
    if (iocp_threads < 1) iocp_threads = 1;
    if (handler_threads < 1) handler_threads = 1;
    spm->config_iocp_threads = iocp_threads;
    spm->config_handler_threads = handler_threads;
}

ServerStats_t krs_server_get_stats(const ServerPortManager_t* spm) {
    ServerStats_t stats = {0};
    if (!spm) return stats;

    stats.messages_received = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_messages_received, 0);
    stats.messages_sent = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_messages_sent, 0);
    stats.ack_sent = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_ack_sent, 0);
    stats.ack_received = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_ack_received, 0);
    stats.retransmissions = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_retransmissions, 0);
    stats.connections_total = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_connections_total, 0);
    stats.disconnections = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_disconnections, 0);
    stats.fragments_received = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_fragments_received, 0);
    stats.fragments_reassembled = (uint64_t)InterlockedOr64((volatile LONG64*)&spm->stat_fragments_reassembled, 0);

    stats.connections_active = stats.connections_total - stats.disconnections;

    if (spm->message_pool) {
        stats.pool_fallback_mallocs = krs_message_pool_get_fallback_count(spm->message_pool);
        stats.pool_acquires = stats.messages_received;
    }

    return stats;
}
