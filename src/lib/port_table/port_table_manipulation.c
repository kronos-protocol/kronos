#include "kronos_port_table.h"
#include "port_table_internal.h"

boolean port_table_insert(PortTable_t* port_table, Port_t port) {
    size_t index = HASH_PORT_INDEX(port, port_table->table_size);
    PortLink_t* port_link = malloc(sizeof(PortLink_t));
    port_link->port = port;
    port_link->next = NULL;

    port_table->total_entries++;

    if (port_table->table[index] == NULL) {
        port_table->table[index] = port_link;
    } else {
        PortLink_t* current_port_link = port_table->table[index];
        while (current_port_link->next != NULL) {
            if (current_port_link->port == port) {
                return false;
            }
            current_port_link = current_port_link->next;
        }
        current_port_link->next = port_link;
    }
    return true;
}

void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port) {
    if ((double)port_table->total_entries / port_table->table_size > MAX_LOAD_FACTOR) {
        port_table_rebuild(port_table);
        port_table_insert(port_table, port);
        return;
    }

    port_table_insert(port_table, port);
}

void port_table_rebuild(PortTable_t* port_table) {

}
