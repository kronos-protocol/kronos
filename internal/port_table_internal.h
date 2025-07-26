#ifndef PORT_TABLE_INTERNAL_H
#define PORT_TABLE_INTERNAL_H

#include "kronos_port_table.h"

typedef struct PortLink PortLink_t;

struct PortTable {
    PortLink_t** table;
    size_t ports_size;
    size_t ports_length;
};

struct PortLink {
    Port_t* port;
    PortLink_t* next;
};

#endif //PORT_TABLE_INTERNAL_H
