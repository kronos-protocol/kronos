#include "kronos_log.h"
#include "kronos_port_table.h"
#include "port_table_internal.h"


PortTableLookup_r krs_lib_port_table_lookup(PortTable_t* port_table, Port_t port) {
    PortTableLookup_r result;
    result.exists = false;
    result.socket_handler = NULL;
    result.base.error_code = KRS_SUCCESS;

    if (!port_table) {
        char* msg = "port_table is NULL";
        KRS_LOG_ERROR(CMP_ID, "%s: { %u }", msg, port_table);
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "%s", msg);
        return result;
    }

    result.base = krs_lib_error_result_base_suc();

    const int start_index = hash_port_index(port, port_table->table_size);

    const PortLink_t* pl = port_table->table[start_index];
    while (pl) {
        if (pl->port == port) {
            result.exists = true;
            result.socket_handler = pl->socket_handler;
            return result;
        }
        pl = pl->next;
    }

    KRS_LOG_ERROR(CMP_ID, "port was not found in port_table: { %u ; %u }", port_table, port);

    return result;
}
