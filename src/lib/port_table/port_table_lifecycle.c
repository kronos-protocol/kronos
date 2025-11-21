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
