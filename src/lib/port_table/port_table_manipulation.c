#include "kronos_port_table.h"
#include "port_table_internal.h"

bool port_table_insert(PortTable_t* port_table, Port_t port) {
    size_t index = HASH_PORT_INDEX(port, port_table->table_size);
    PortLink_t* port_link = malloc(sizeof(PortLink_t));
    port_link->port = port;
    //TODO: initialize SocketHandler for Port
    port_link->next = NULL;

    port_table->total_entries++;

    if (port_table->table[index] == NULL) {
        port_table->table[index] = port_link;
    } else {
        PortLink_t* current_port_link = port_table->table[index];

        while (current_port_link != NULL) {
            if (current_port_link->port == port) {
                free(port_link);
                port_table->total_entries--;
                return false;
            }

            if (current_port_link->next == NULL) {
                current_port_link->next = port_link;
                break;
            }

            current_port_link = current_port_link->next;
        }
    }
    return true;
}

void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port) {
    //TODO: rebuild is called even if the port already exists
    if ((double)port_table->total_entries / port_table->table_size > MAX_LOAD_FACTOR) {
        port_table_rebuild(port_table);
        port_table_insert(port_table, port);
        return;
    }

    port_table_insert(port_table, port);
}

void port_table_rebuild(PortTable_t* port_table) {
    PortLink_t** old_port_links = port_table->table;
    uint32_t old_table_size = port_table->table_size;
    uint8_t next_index = port_table->prime_size_index + 1;

    if (next_index >= PRIME_SIZES_COUNT) {
        return;
    }

    uint32_t new_table_size = PRIME_SIZES[next_index];
    PortLink_t** new_table = calloc(new_table_size, sizeof(PortLink_t*));
    port_table->table = new_table;
    port_table->table_size = new_table_size;
    port_table->prime_size_index = next_index;
    port_table->total_entries = 0;

    for (int i = 0; i < old_table_size; i++) {
        PortLink_t* current_port_link = old_port_links[i];

        while (current_port_link != NULL) {
            port_table_insert(port_table, current_port_link->port);
            PortLink_t* removable_port_link = current_port_link;
            current_port_link = current_port_link->next;
            free(removable_port_link);
        }
    }

    free(old_port_links);
}
