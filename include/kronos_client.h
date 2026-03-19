#ifndef KRONOS_CLIENT_H
#define KRONOS_CLIENT_H

#include "kronos_network.h"
#include "kronos_error.h"

#include <stdint.h>
#include <stdbool.h>

/** @brief Opaque client-side connection to a remote server. */
typedef struct ServerConnection ServerConnection_t;

/**
 * @brief Initiates a connection to a remote Kronos server.
 *
 * Sends a CONNECTION frame on channel 0 and waits for a SOCKET_ACK response.
 * Allocates a UDP socket bound to an ephemeral local port.
 *
 * @param server_address  The remote server address and port to connect to.
 * @return Pointer to a new ServerConnection_t on success, or NULL on failure.
 */
ServerConnection_t* krs_client_server_connect(PortAddress_t server_address);

/**
 * @brief Sends data to the server on the specified channel.
 *
 * @param conn         The active server connection.
 * @param channel      Destination channel (must be >= 10 for application channels).
 * @param data         Payload bytes to send.
 * @param length       Number of payload bytes.
 * @param require_ack  If true, sends with ACK tracking (not yet implemented).
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                  Data sent.
 * @retval KRS_ERR_NULL_POINTER         conn or data is NULL.
 * @retval KRS_ERR_CLIENT_NOT_CONNECTED conn is not in connected state.
 * @retval KRS_ERR_NETWORK_SOCKET_ERROR WSASendTo failed.
 */
Void_r krs_client_send(ServerConnection_t* conn, Channel_t channel,
                       const uint8_t* data, uint16_t length, bool require_ack);

/**
 * @brief Disconnects from the server and frees the connection.
 *
 * Closes the underlying UDP socket and sets *conn to NULL.
 *
 * @param conn  Pointer to the connection pointer; set to NULL on return.
 */
void krs_client_disconnect(ServerConnection_t** conn);

#endif // KRONOS_CLIENT_H
