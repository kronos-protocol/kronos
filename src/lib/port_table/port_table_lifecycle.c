#include "kronos_port_table.h"
#include "port_table_internal.h"

PortTable_t* krs_lib_port_table_create() {
    PortTable_t* port_table = malloc(sizeof(PortTable_t));
    uint32_t initial_size = PRIME_SIZES[0];
    PortLink_t** port_link = malloc(initial_size * sizeof(PortLink_t));

    port_table->table = port_link;
    port_table->table_size = initial_size;

    return port_table;
}
