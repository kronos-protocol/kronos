#ifndef PORT_TABLE_INTERNAL_H
#define PORT_TABLE_INTERNAL_H

#include "kronos_port_table.h"
#include "kronos_server.h"

typedef struct PortLink PortLink_t;

struct PortTable {
    PortLink_t** table;
    size_t table_size;
    uint8_t prime_size_index;
    uint32_t total_entries;
};

struct PortLink {
    UdpSocketHandler_t* socket_handler;
    Port_t port;
    PortLink_t* next;
};

void port_table_rebuild(PortTable_t* port_table);

int hash_port_index(int port, int size);

#define HASH_MULTIPLIER 2654435761u

static const uint32_t PRIME_SIZES[] = {
    17u, 31u, 67u, 131u, 257u, 521u, 1031u, 2053u,
    4099u, 8209u, 16411u, 32771u, 65537u
};

#define PRIME_SIZES_COUNT (sizeof(PRIME_SIZES) / sizeof(PRIME_SIZES[0]))

#define MAX_LOAD_FACTOR 1.5

#define HASH_PORT_INDEX(port, size) ((port) * HASH_MULTIPLIER % (size))

#endif //PORT_TABLE_INTERNAL_H