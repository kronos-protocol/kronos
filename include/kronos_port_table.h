#ifndef KRONOS_PORT_TABLE_H
#define KRONOS_PORT_TABLE_H

#include "kronos_network.h"
#include "kronos_server.h"
#include "kronos_error.h"

#include <stdlib.h>
#include <stdbool.h>

typedef struct PortTable PortTable_t;
typedef struct PortTableCreateResult PortTableCreate_r;

typedef void (*socket_handler_destroy_f) (UdpSocketHandler_t** socket_handler);

struct PortTableCreateResult {
    KronosResult_b base;
    PortTable_t* port_table;
};

PortTable_t* krs_lib_port_table_create();
PortTableCreate_r krs_lib_port_table_create_s();

void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port, UdpSocketHandler_t* udp_socket_handler);
Void_r krs_lib_port_table_insert_s(PortTable_t* port_table, Port_t port);

void krs_lib_port_table_destroy(PortTable_t** port_table);

#endif //KRONOS_PORT_TABLE_H
