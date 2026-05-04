#include "kronos_ack.h"
#include "kronos_congestion.h"

#include "connection_map_internal.h"
#include "server_internal.h"

#include <stdlib.h>
#include <string.h>


static uint32_t s_next_power_of_two(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v < 16 ? 16 : v;
}

static uint32_t s_hash_address(const PortAddress_t* addr) {
    uint32_t hash = 2166136261u;
    hash ^= (uint32_t)addr->sin6_port;
    hash *= 16777619u;
    for (int i = 0; i < 16; i++) {
        hash ^= addr->sin6_addr.s6_addr[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t s_addr_probe(const ConnectionMap_t* map, const PortAddress_t* addr) {
    return s_hash_address(addr) & (map->addr_capacity - 1);
}

static void s_addr_grow(ConnectionMap_t* map) {
    uint32_t old_cap = map->addr_capacity;
    AddressMapEntry_t* old_entries = map->addr_entries;

    uint32_t new_cap = old_cap * 2;
    AddressMapEntry_t* new_entries = calloc(new_cap, sizeof(AddressMapEntry_t));
    if (!new_entries) return;

    map->addr_entries = new_entries;
    map->addr_capacity = new_cap;
    map->addr_count = 0;

    for (uint32_t i = 0; i < old_cap; i++) {
        if (old_entries[i].occupied && !old_entries[i].deleted) {
            uint32_t idx = s_hash_address(&old_entries[i].address) & (new_cap - 1);
            while (new_entries[idx].occupied) {
                idx = (idx + 1) & (new_cap - 1);
            }
            new_entries[idx] = old_entries[i];
            new_entries[idx].deleted = false;
            map->addr_count++;
        }
    }

    free(old_entries);
}

static void s_addr_put(ConnectionMap_t* map, const PortAddress_t* addr, uint32_t connection_id) {
    if (map->addr_count * 4 >= map->addr_capacity * 3) {
        s_addr_grow(map);
    }

    uint32_t idx = s_addr_probe(map, addr);
    while (map->addr_entries[idx].occupied && !map->addr_entries[idx].deleted) {
        if (krs_network_port_address_equals(&map->addr_entries[idx].address, addr)) {
            map->addr_entries[idx].connection_id = connection_id;
            return;
        }
        idx = (idx + 1) & (map->addr_capacity - 1);
    }

    map->addr_entries[idx].address = *addr;
    map->addr_entries[idx].connection_id = connection_id;
    map->addr_entries[idx].occupied = true;
    map->addr_entries[idx].deleted = false;
    map->addr_count++;
}

static void s_addr_remove(ConnectionMap_t* map, const PortAddress_t* addr) {
    uint32_t idx = s_addr_probe(map, addr);
    uint32_t checked = 0;
    while (map->addr_entries[idx].occupied && checked < map->addr_capacity) {
        if (!map->addr_entries[idx].deleted &&
            krs_network_port_address_equals(&map->addr_entries[idx].address, addr)) {
            map->addr_entries[idx].deleted = true;
            map->addr_count--;
            return;
        }
        idx = (idx + 1) & (map->addr_capacity - 1);
        checked++;
    }
}

ConnectionMap_t* krs_connection_map_create(uint32_t initial_capacity) {
    ConnectionMap_t* map = calloc(1, sizeof(ConnectionMap_t));
    if (!map) return NULL;

    map->capacity = s_next_power_of_two(initial_capacity);
    map->entries = calloc(map->capacity, sizeof(ConnectionMapEntry_t));
    if (!map->entries) {
        free(map);
        return NULL;
    }

    map->addr_capacity = map->capacity;
    map->addr_entries = calloc(map->addr_capacity, sizeof(AddressMapEntry_t));
    if (!map->addr_entries) {
        free(map->entries);
        free(map);
        return NULL;
    }

    InitializeSRWLock(&map->lock);
    return map;
}

void krs_connection_map_destroy(ConnectionMap_t** map) {
    if (!map || !*map) return;
    free((*map)->entries);
    free((*map)->addr_entries);
    free(*map);
    *map = NULL;
}

static uint32_t s_probe(const ConnectionMap_t* map, uint32_t connection_id) {
    return connection_id & (map->capacity - 1);
}

static void s_grow(ConnectionMap_t* map) {
    uint32_t old_cap = map->capacity;
    ConnectionMapEntry_t* old_entries = map->entries;

    uint32_t new_cap = old_cap * 2;
    ConnectionMapEntry_t* new_entries = calloc(new_cap, sizeof(ConnectionMapEntry_t));
    if (!new_entries) return;

    map->entries = new_entries;
    map->capacity = new_cap;
    map->count = 0;

    for (uint32_t i = 0; i < old_cap; i++) {
        if (old_entries[i].occupied && !old_entries[i].deleted) {
            uint32_t idx = old_entries[i].connection_id & (new_cap - 1);
            while (new_entries[idx].occupied) {
                idx = (idx + 1) & (new_cap - 1);
            }
            new_entries[idx] = old_entries[i];
            new_entries[idx].deleted = false;
            map->count++;
        }
    }

    free(old_entries);
}

void krs_connection_map_put(ConnectionMap_t* map, uint32_t connection_id,
                            UDPSocketDescriptor_t* desc, ClientConnection_t* conn) {
    if (!map) return;

    if (map->count * 4 >= map->capacity * 3) {
        s_grow(map);
    }

    uint32_t idx = s_probe(map, connection_id);
    while (map->entries[idx].occupied && !map->entries[idx].deleted) {
        if (map->entries[idx].connection_id == connection_id) {
            map->entries[idx].descriptor = desc;
            map->entries[idx].connection = conn;
            s_addr_put(map, &conn->remote_address, connection_id);
            return;
        }
        idx = (idx + 1) & (map->capacity - 1);
    }

    map->entries[idx].connection_id = connection_id;
    map->entries[idx].descriptor = desc;
    map->entries[idx].connection = conn;
    map->entries[idx].occupied = true;
    map->entries[idx].deleted = false;
    map->count++;
    s_addr_put(map, &conn->remote_address, connection_id);
}

ConnectionMapEntry_t* krs_connection_map_get(ConnectionMap_t* map, uint32_t connection_id) {
    if (!map) return NULL;

    uint32_t idx = s_probe(map, connection_id);
    uint32_t checked = 0;
    while (map->entries[idx].occupied && checked < map->capacity) {
        if (!map->entries[idx].deleted && map->entries[idx].connection_id == connection_id) {
            return &map->entries[idx];
        }
        idx = (idx + 1) & (map->capacity - 1);
        checked++;
    }
    return NULL;
}

void krs_connection_map_remove(ConnectionMap_t* map, uint32_t connection_id) {
    if (!map) return;

    uint32_t idx = s_probe(map, connection_id);
    uint32_t checked = 0;
    while (map->entries[idx].occupied && checked < map->capacity) {
        if (!map->entries[idx].deleted && map->entries[idx].connection_id == connection_id) {
            if (map->entries[idx].connection) {
                s_addr_remove(map, &map->entries[idx].connection->remote_address);
            }
            map->entries[idx].deleted = true;
            map->count--;
            return;
        }
        idx = (idx + 1) & (map->capacity - 1);
        checked++;
    }
}

uint32_t krs_connection_map_get_by_address(ConnectionMap_t* map, const PortAddress_t* addr) {
    if (!map || !addr) return 0;

    uint32_t idx = s_addr_probe(map, addr);
    uint32_t checked = 0;
    while (map->addr_entries[idx].occupied && checked < map->addr_capacity) {
        if (!map->addr_entries[idx].deleted &&
            krs_network_port_address_equals(&map->addr_entries[idx].address, addr)) {
            return map->addr_entries[idx].connection_id;
        }
        idx = (idx + 1) & (map->addr_capacity - 1);
        checked++;
    }
    return 0;
}

bool krs_connection_map_acquire(ConnectionMap_t* map, uint32_t connection_id,
                                UDPSocketDescriptor_t** desc_out, ClientConnection_t** conn_out) {
    if (!map || !desc_out || !conn_out) return false;

    AcquireSRWLockShared(&map->lock);
    ConnectionMapEntry_t* entry = krs_connection_map_get(map, connection_id);
    if (!entry || !entry->connection) {
        ReleaseSRWLockShared(&map->lock);
        return false;
    }

    *desc_out = entry->descriptor;
    *conn_out = entry->connection;
    InterlockedIncrement(&entry->connection->refcount);
    ReleaseSRWLockShared(&map->lock);
    return true;
}

void krs_connection_map_release(ClientConnection_t* conn) {
    if (!conn) return;
    if (InterlockedDecrement(&conn->refcount) != 0) return;

    krs_ack_tracker_destroy(&conn->ack_tracker);
    krs_congestion_destroy(&conn->congestion);
    free(conn);
}
