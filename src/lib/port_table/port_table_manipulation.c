#include "kronos_port_table.h"
#include "port_table_internal.h"

void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port) {
    if (port_table->total_entries / port_table->table_size > MAX_LOAD_FACTOR) {
        port_table_rebuild(port_table);
        krs_lib_port_table_insert(port_table, port);
        return;
    }

    size_t index = HASH_PORT_INDEX(port, port_table->table_size);

    if (port_table->table[index] == NULL) {

    } else {

    }
}
