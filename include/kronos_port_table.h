#ifndef KRONOS_PORT_TABLE_H
#define KRONOS_PORT_TABLE_H

#include "kronos_network.h"

typedef struct PortTable PortTable_t;

PortTable_t* krs_lib_port_table_create();

void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port);

#endif //KRONOS_PORT_TABLE_H
