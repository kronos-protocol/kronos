#include "port_table_internal.h"

// This file is used to link the hash_port_index function to not directly use the macro, so that cmock can mock it
int hash_port_index(uint32_t port, uint32_t size) {
    return HASH_PORT_INDEX(port, size);
}