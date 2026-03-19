#ifndef KRONOS_PORT_TABLE_H
#define KRONOS_PORT_TABLE_H

#include "kronos_network.h"
#include "kronos_server.h"
#include "kronos_error.h"

#include <stdlib.h>
#include <stdbool.h>

/** @brief Component identifier used in log messages from the port table module. */
#define CMP_ID "LIB-PORT-TABLE"

/** @brief Opaque hash table mapping port numbers to UDPSocketDescriptor_t instances. */
typedef struct PortTable PortTable_t;

/** @brief Result type for port table creation. */
typedef struct PortTableCreateResult PortTableCreate_r;

/** @brief Result type for port table lookup operations. */
typedef struct PortTableLookupResult PortTableLookup_r;

struct PortTableCreateResult {
    KronosResult_b base;
    PortTable_t* port_table;
};

struct PortTableLookupResult {
    KronosResult_b base;
    bool exists;
    UDPSocketDescriptor_t* socket_handler;
};

/**
 * @brief Creates a new empty port table.
 *
 * @return Pointer to the new PortTable_t, or NULL on allocation failure.
 */
PortTable_t* krs_lib_port_table_create(void);

/**
 * @brief Creates a new empty port table with explicit error handling.
 *
 * @return PortTableCreate_r containing the table or error information.
 *
 * @retval KRS_SUCCESS                Table created successfully.
 * @retval KRS_ERR_MEMORY_ALLOCATION  Allocation failed.
 */
PortTableCreate_r krs_lib_port_table_create_s(void);

/**
 * @brief Inserts a port-to-descriptor mapping into the table.
 *
 * Rebuilds the hash table if the load factor exceeds the threshold.
 * Does nothing if port_table or udp_socket_handler is NULL.
 *
 * @param port_table          The port table to insert into.
 * @param port                The port number to map.
 * @param udp_socket_handler  The descriptor to associate with the port.
 */
void krs_lib_port_table_insert(PortTable_t* port_table, Port_t port, UDPSocketDescriptor_t* udp_socket_handler);

/**
 * @brief Inserts a port-to-descriptor mapping with explicit error handling.
 *
 * @param port_table          The port table to insert into.
 * @param port                The port number to map.
 * @param udp_socket_handler  The descriptor to associate with the port.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS            Insertion succeeded.
 * @retval KRS_ERR_NULL_POINTER   port_table or udp_socket_handler is NULL.
 */
Void_r krs_lib_port_table_insert_s(PortTable_t* port_table, Port_t port, UDPSocketDescriptor_t* udp_socket_handler);

/**
 * @brief Looks up the descriptor associated with a port number.
 *
 * @param port_table  The port table to search.
 * @param port        The port number to look up.
 * @return PortTableLookup_r with exists=true and a non-NULL socket_handler if found.
 *
 * @retval KRS_SUCCESS           Lookup completed (check .exists for presence).
 * @retval KRS_ERR_NULL_POINTER  port_table is NULL.
 */
PortTableLookup_r krs_lib_port_table_lookup(PortTable_t* port_table, Port_t port);

/**
 * @brief Destroys a port table and all associated socket descriptors.
 *
 * Calls krs_server_udp_socket_handler_destroy() on each entry before freeing.
 *
 * @param port_table  Pointer to the table pointer; set to NULL on return.
 */
void krs_lib_port_table_destroy(PortTable_t** port_table);

#endif // KRONOS_PORT_TABLE_H
