#include "kronos.h"
#include "kronos_server.h"
#include "kronos_network.h"

#include "connection_map_internal.h"
#include "server_internal.h"

#include "unity.h"
#include <stdlib.h>
#include <string.h>


static ClientConnection_t* s_make_dummy_conn(uint32_t id, uint16_t port) {
    ClientConnection_t* c = calloc(1, sizeof(ClientConnection_t));
    if (!c) return NULL;
    c->connection_id = id;
    c->refcount = 1;
    c->remote_address.sin6_family = AF_INET6;
    c->remote_address.sin6_port = port;
    InitializeSRWLock(&c->ack_lock);
    InitializeSRWLock(&c->cc_lock);
    return c;
}

static void s_free_dummy_conn(ClientConnection_t* c) {
    free(c);
}

void test_connection_map_remove_increments_tombstones(void) {
    ConnectionMap_t* map = krs_connection_map_create(16);
    TEST_ASSERT_NOT_NULL(map);

    ClientConnection_t* c1 = s_make_dummy_conn(1, 1001);
    krs_connection_map_put(map, 1, NULL, c1);
    TEST_ASSERT_EQUAL_UINT32(1, map->count);
    TEST_ASSERT_EQUAL_UINT32(0, map->tombstones);

    krs_connection_map_remove(map, 1);
    TEST_ASSERT_EQUAL_UINT32(0, map->count);
    TEST_ASSERT_EQUAL_UINT32(1, map->tombstones);

    s_free_dummy_conn(c1);
    krs_connection_map_destroy(&map);
}

void test_connection_map_insert_recycles_tombstone(void) {
    ConnectionMap_t* map = krs_connection_map_create(16);

    ClientConnection_t* c1 = s_make_dummy_conn(1, 1001);
    krs_connection_map_put(map, 1, NULL, c1);
    krs_connection_map_remove(map, 1);

    TEST_ASSERT_EQUAL_UINT32(0, map->count);
    TEST_ASSERT_EQUAL_UINT32(1, map->tombstones);

    ClientConnection_t* c17 = s_make_dummy_conn(17, 1017);
    krs_connection_map_put(map, 17, NULL, c17);

    TEST_ASSERT_EQUAL_UINT32(1, map->count);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, map->tombstones,
        "tombstone should have been recycled by the new insert");

    ConnectionMapEntry_t* entry = krs_connection_map_get(map, 17);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_UINT32(17, entry->connection_id);

    s_free_dummy_conn(c1);
    s_free_dummy_conn(c17);
    krs_connection_map_destroy(&map);
}

void test_connection_map_grow_resets_tombstones(void) {
    ConnectionMap_t* map = krs_connection_map_create(16);
    uint32_t initial_capacity = map->capacity;

    ClientConnection_t* conns[16];
    for (int i = 0; i < 8; i++) {
        conns[i] = s_make_dummy_conn((uint32_t)(i + 1), (uint16_t)(2000 + i));
        krs_connection_map_put(map, (uint32_t)(i + 1), NULL, conns[i]);
    }
    for (int i = 0; i < 4; i++) {
        krs_connection_map_remove(map, (uint32_t)(i + 1));
    }
    TEST_ASSERT_EQUAL_UINT32(4, map->count);
    TEST_ASSERT_EQUAL_UINT32(4, map->tombstones);

    for (int i = 8; i < 13; i++) {
        conns[i] = s_make_dummy_conn((uint32_t)(i + 100), (uint16_t)(2000 + i));
        krs_connection_map_put(map, (uint32_t)(i + 100), NULL, conns[i]);
    }

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(initial_capacity, map->capacity,
        "map should have grown when (count+tombstones) crossed the trigger");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, map->tombstones,
        "tombstones must be zero immediately after grow");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(9, map->count,
        "live entries should be preserved across grow (4 surviving + 5 new)");

    for (int i = 0; i < 8; i++) s_free_dummy_conn(conns[i]);
    for (int i = 8; i < 13; i++) s_free_dummy_conn(conns[i]);
    krs_connection_map_destroy(&map);
}

void test_connection_map_high_churn_capacity_bounded(void) {
    ConnectionMap_t* map = krs_connection_map_create(16);

    enum { ROUNDS = 1000, LIVE_CONNECTIONS = 50 };
    ClientConnection_t* live[LIVE_CONNECTIONS];
    uint32_t next_id = 1;

    for (int i = 0; i < LIVE_CONNECTIONS; i++) {
        live[i] = s_make_dummy_conn(next_id, (uint16_t)(3000 + i));
        krs_connection_map_put(map, next_id, NULL, live[i]);
        next_id++;
    }

    for (int r = 0; r < ROUNDS; r++) {
        int slot = r % LIVE_CONNECTIONS;
        krs_connection_map_remove(map, live[slot]->connection_id);
        s_free_dummy_conn(live[slot]);
        live[slot] = s_make_dummy_conn(next_id, (uint16_t)(4000 + slot));
        krs_connection_map_put(map, next_id, NULL, live[slot]);
        next_id++;
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(LIVE_CONNECTIONS, map->count,
        "live count should match what was inserted");

    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(2048, map->capacity,
        "capacity should not balloon under steady-state churn");
    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(map->capacity, map->tombstones * 2,
        "tombstone count should be bounded relative to capacity");

    for (int i = 0; i < LIVE_CONNECTIONS; i++) {
        ConnectionMapEntry_t* e = krs_connection_map_get(map, live[i]->connection_id);
        TEST_ASSERT_NOT_NULL_MESSAGE(e, "every live connection must be findable");
        TEST_ASSERT_EQUAL_UINT32(live[i]->connection_id, e->connection_id);
    }

    for (int i = 0; i < LIVE_CONNECTIONS; i++) s_free_dummy_conn(live[i]);
    krs_connection_map_destroy(&map);
}

void test_connection_map_put_same_id_updates_in_place(void) {
    ConnectionMap_t* map = krs_connection_map_create(16);

    ClientConnection_t* c1a = s_make_dummy_conn(42, 5001);
    ClientConnection_t* c1b = s_make_dummy_conn(42, 5002);
    krs_connection_map_put(map, 42, NULL, c1a);
    krs_connection_map_put(map, 42, NULL, c1b);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, map->count,
        "putting the same ID twice must not duplicate");

    ConnectionMapEntry_t* entry = krs_connection_map_get(map, 42);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(c1b, entry->connection,
        "the second put should overwrite the first");

    s_free_dummy_conn(c1a);
    s_free_dummy_conn(c1b);
    krs_connection_map_destroy(&map);
}
