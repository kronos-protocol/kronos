#ifndef CONNECTION_MAP_INTERNAL_H
#define CONNECTION_MAP_INTERNAL_H

#include "kronos_network.h"
#include <stdint.h>
#include <stdbool.h>
#include <winsock2.h>

typedef struct UDPSocketDescriptor UDPSocketDescriptor_t;
typedef struct ClientConnection ClientConnection_t;

typedef struct ConnectionMapEntry ConnectionMapEntry_t;
typedef struct ConnectionMap ConnectionMap_t;

struct ConnectionMapEntry {
    uint32_t connection_id;
    UDPSocketDescriptor_t* descriptor;
    ClientConnection_t* connection;
    bool occupied;
    bool deleted;
};

typedef struct AddressMapEntry AddressMapEntry_t;

struct AddressMapEntry {
    PortAddress_t address;
    uint32_t connection_id;
    bool occupied;
    bool deleted;
};

struct ConnectionMap {
    ConnectionMapEntry_t* entries;
    uint32_t capacity;
    uint32_t count;
    AddressMapEntry_t* addr_entries;
    uint32_t addr_capacity;
    uint32_t addr_count;
    SRWLOCK lock;
};

ConnectionMap_t* krs_connection_map_create(uint32_t initial_capacity);
void krs_connection_map_destroy(ConnectionMap_t** map);
void krs_connection_map_put(ConnectionMap_t* map, uint32_t connection_id,
                            UDPSocketDescriptor_t* desc, ClientConnection_t* conn);
ConnectionMapEntry_t* krs_connection_map_get(ConnectionMap_t* map, uint32_t connection_id);
void krs_connection_map_remove(ConnectionMap_t* map, uint32_t connection_id);
uint32_t krs_connection_map_get_by_address(ConnectionMap_t* map, const PortAddress_t* addr);

bool krs_connection_map_acquire(ConnectionMap_t* map, uint32_t connection_id,
                                UDPSocketDescriptor_t** desc_out, ClientConnection_t** conn_out);
void krs_connection_map_release(ClientConnection_t* conn);

#endif // CONNECTION_MAP_INTERNAL_H
