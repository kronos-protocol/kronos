#include "kronos_server.h"
#include "server_internal.h"

#include <stdlib.h>


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
    return server_port_manager;
}

void krs_server_port_manager_port_add(ServerPortManager_t* spm, Port_t port) {
    if (!krs_server_port_manager_validate(spm)) return;

    PortAddress_t port_address = krs_network_port_address_create(port, spm->default_address);
    UDPSocketDescriptor_t* socket_handler = krs_server_udp_socket_handler_create(port_address);
    if (!socket_handler) return;
    socket_handler->port = port;
    krs_lib_port_table_insert(spm->port_table, port, socket_handler);
    krs_array_push(spm->descriptor_list, socket_handler);
}

void krs_server_port_manager_port_add_with_address(ServerPortManager_t* spm, Port_t port, Address_t address) {
    if (!krs_server_port_manager_validate(spm)) return;

    PortAddress_t port_address = krs_network_port_address_create(port, address);
    UDPSocketDescriptor_t* socket_handler = krs_server_udp_socket_handler_create(port_address);
    if (!socket_handler) return;
    socket_handler->port = port;
    krs_lib_port_table_insert(spm->port_table, port, socket_handler);
    krs_array_push(spm->descriptor_list, socket_handler);
}

void krs_server_port_manager_destroy(ServerPortManager_t** spm) {
    if (!spm || !*spm) return;
    ServerPortManager_t* m = *spm;
    krs_array_destroy(&m->descriptor_list);
    krs_lib_port_table_destroy(&m->port_table);
    free(m);
    *spm = NULL;
}
