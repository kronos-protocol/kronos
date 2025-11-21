#include "kronos_port_table.h"
#include "port_table_internal.h"

PortTable_t* krs_lib_port_table_create(void) {
    PortTable_t* pt = malloc(sizeof(PortTable_t));
    if (!pt) return NULL;

    uint32_t initial_size = PRIME_SIZES[0];
    pt->table = calloc(initial_size, sizeof(PortLink_t*));
    if (!pt->table) {
        free(pt);
        return NULL;
    }

    pt->table_size = initial_size;
    pt->total_entries = 0;
    pt->prime_size_index = 0;
    return pt;
}

void krs_lib_port_table_destroy(PortTable_t** ptp) {
    if (!ptp || !*ptp) return;

    PortTable_t* pt = *ptp;

    for (int i = 0; i < pt->table_size; i++) {
        PortLink_t* link = pt->table[i];
        while (link != NULL) {
            PortLink_t* next = link->next;
            free(link->socket_handler); //TODO: use function for this once socket handlers are implemented
            free(link);
            link = next;
        }
    }

    free(pt->table);
    free(pt);
    *ptp = NULL;
}
