#include "kronos_port_table.h"
#include "port_table_internal.h"

static bool port_table_insert(PortTable_t* port_table, Port_t port, UDPSocketDescriptor_t* udp_socket_handler) {
    if (!port_table || !udp_socket_handler) return false;

    size_t index = hash_port_index(port, port_table->table_size);
    PortLink_t* port_link = malloc(sizeof(PortLink_t));
    if (!port_link) return false;

    port_link->port = port;
    port_link->socket_handler = udp_socket_handler;
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

void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port, UDPSocketDescriptor* udp_socket_handler) {
    if (!port_table || !udp_socket_handler) return;

    if (!port_table->table) return;

    //TODO: rebuild is called even if the port already exists
    if ((double)port_table->total_entries / port_table->table_size > MAX_LOAD_FACTOR) {
        port_table_rebuild(port_table);
        port_table_insert(port_table, port, udp_socket_handler);
        return;
    }

    port_table_insert(port_table, port, udp_socket_handler);
}

void port_table_rebuild(PortTable_t* port_table) {
    if (port_table == NULL) return;

    PortLink_t** old_port_links = port_table->table;
    uint32_t old_table_size = port_table->table_size;
    uint8_t next_index = port_table->prime_size_index + 1;

    if (next_index >= PRIME_SIZES_COUNT) {
        return;
    }

    uint32_t new_table_size = PRIME_SIZES[next_index];
    PortLink_t** new_table = calloc(new_table_size, sizeof(PortLink_t*));

    if (new_table == NULL) return;

    port_table->table = new_table;
    port_table->table_size = new_table_size;
    port_table->prime_size_index = next_index;
    port_table->total_entries = 0;

    for (int i = 0; i < old_table_size; i++) {
        PortLink_t* current_port_link = old_port_links[i];

        while (current_port_link != NULL) {
            port_table_insert(port_table, current_port_link->port, current_port_link->socket_handler);
            PortLink_t* removable_port_link = current_port_link;
            current_port_link = current_port_link->next;
            free(removable_port_link);
        }
    }

    free(old_port_links);
}
