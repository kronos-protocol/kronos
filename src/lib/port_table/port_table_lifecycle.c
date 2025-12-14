#include "kronos_port_table.h"
#include "port_table_internal.h"

static PortTable_t* port_table_alloc(void) {
    PortTable_t* pt = malloc(sizeof(PortTable_t));
    if (pt) {
        pt->total_entries = 0;
        pt->prime_size_index = 0;
        pt->table_size = 0;
    }
    return pt;
}

static void port_links_alloc(PortTable_t* pt) {
    const uint32_t initial_size = PRIME_SIZES[0];
    PortLink_t** port_links = calloc(initial_size, sizeof(PortLink_t*));

    if (port_links) {
        pt->table = port_links;
        pt->table_size = initial_size;
    } else {
        pt->table = NULL;
    }
}

static PortTable_t* port_table_create(void) {
    PortTable_t* pt = port_table_alloc();
    if (!pt) return NULL;

    port_links_alloc(pt);

    if (!pt->table) {
        free(pt);
        return NULL;
    }

    return pt;
}


PortTable_t* krs_lib_port_table_create(void) {
    return port_table_create();
}

PortTableCreate_r krs_lib_port_table_create_s(void) {
    PortTableCreate_r result;
    result.port_table = NULL;

    PortTable_t* pt = port_table_create();

    if (!pt) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "Could not allocate memory for PortTable_t");
        return result;
    }

    port_links_alloc(pt);

    if (!pt->table) {
        free(pt);
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION, "Could not allocate memory for PortLink_t");
        return result;
    }

    result.base = krs_lib_error_result_base_suc();
    result.port_table = pt;
    return result;
}

void krs_lib_port_table_destroy(PortTable_t** ptp) {
    if (!ptp || !*ptp) return;

    PortTable_t* pt = *ptp;

    for (int i = 0; i < pt->table_size; i++) {
        PortLink_t* link = pt->table[i];
        while (link != NULL) {
            PortLink_t* next = link->next;
            krs_server_udp_socket_handler_destroy(&link->socket_handler);
            free(link);
            link = next;
        }
    }

    free(pt->table);
    free(pt);
    *ptp = NULL;
}
