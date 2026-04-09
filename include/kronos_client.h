#ifndef KRONOS_CLIENT_H
#define KRONOS_CLIENT_H

#include "kronos_network.h"
#include "kronos_error.h"
#include "kronos_server.h"

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
 * @param require_ack  If true, sets META_FLAG_ACK_REQUIRED and registers the
 *                     packet for retransmission via the client's AckTracker.
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

/**
 * @brief Registers a callback to receive messages from the server.
 *
 * Must be called before krs_client_start_receive(). The callback is invoked
 * from the client's receive thread whenever a complete application message
 * arrives on any channel >= 10.
 *
 * @param conn       The active server connection.
 * @param callback   Callback function pointer.
 * @param user_data  Caller-supplied context passed to the callback.
 */
void krs_client_set_callback(ServerConnection_t* conn, ChannelMessageCallback_f callback,
                             void* user_data);

/**
 * @brief Starts the client receive thread to listen for server messages.
 *
 * Spawns a background thread that calls recvfrom on the client socket,
 * parses incoming frames, handles reassembly, and invokes the registered
 * callback for complete application messages. Also sends MESSAGE_ACK
 * responses for frames with META_FLAG_ACK_REQUIRED set.
 *
 * @param conn  The active server connection with a registered callback.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                  Receive thread started.
 * @retval KRS_ERR_NULL_POINTER         conn is NULL.
 * @retval KRS_ERR_CLIENT_NOT_CONNECTED conn is not connected.
 * @retval KRS_ERR_ALREADY_INITIALIZED  Receive thread already running.
 */
Void_r krs_client_start_receive(ServerConnection_t* conn);

#endif // KRONOS_CLIENT_H
