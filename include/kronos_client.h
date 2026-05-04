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
 * @note Thread-safe with respect to other concurrent `krs_client_send` calls
 *       on the same connection. NOT safe to call concurrently with
 *       `krs_client_disconnect` — see the `krs_client_disconnect` doc for
 *       the lifecycle contract.
 *
 * @retval KRS_SUCCESS                            Data sent.
 * @retval KRS_ERR_NULL_POINTER                   conn or data is NULL.
 * @retval KRS_ERR_CLIENT_NOT_CONNECTED           conn is not in connected state.
 * @retval KRS_ERR_SERVER_CONGESTION_WINDOW_FULL  require_ack=true and congestion window is full.
 * @retval KRS_ERR_MEMORY_ALLOCATION              Frame builder or fragmentation allocation failed.
 * @retval KRS_ERR_BUFFER_TOO_SMALL               Frame serialization failed (single-frame path).
 * @retval KRS_ERR_INVALID_PARAMETER              MTU too small for fragmentation header (multi-frame path).
 */
Void_r krs_client_send(ServerConnection_t* conn, Channel_t channel,
                       const uint8_t* data, uint16_t length, bool require_ack);

/**
 * @brief Disconnects from the server and frees the connection.
 *
 * Closes the underlying UDP socket and sets *conn to NULL.
 *
 * @warning **Threading contract**: The caller MUST ensure no other thread
 *          is calling `krs_client_send`, `krs_client_send_blocking`,
 *          `krs_client_set_callback`, or `krs_client_set_delivery_failure_callback`
 *          on the same `ServerConnection_t` while `krs_client_disconnect`
 *          executes. After this function returns, `*conn` is NULL and the
 *          memory it referenced is freed. Concurrent access during the call
 *          window has undefined behavior including use-after-free.
 *
 *          The receive thread (started by `krs_client_start_receive`) is
 *          managed by `krs_client_disconnect` itself — the caller does not
 *          need to stop it manually.
 *
 *          Recommended pattern: only one application thread (typically the
 *          main thread) owns the `ServerConnection_t*` lifecycle. Sender
 *          threads should be joined or stopped by the application before
 *          calling `krs_client_disconnect`.
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

/**
 * @brief Sends data reliably with automatic congestion backpressure retry.
 *
 * Calls krs_client_send with require_ack=true. If the congestion window is
 * full, retries with 1ms sleep intervals until success or timeout_ms expires.
 *
 * @param conn        The active server connection.
 * @param channel     Destination channel (must be >= 10).
 * @param data        Payload bytes to send.
 * @param length      Number of payload bytes.
 * @param timeout_ms  Maximum time to wait for the congestion window in milliseconds.
 *                    Pass 0 for a single attempt with no retries.
 * @return Void_r indicating success or failure.
 *
 * @note Thread-safe with respect to other concurrent `krs_client_send` /
 *       `krs_client_send_blocking` calls on the same connection. NOT safe
 *       to call concurrently with `krs_client_disconnect` — see the
 *       `krs_client_disconnect` doc for the lifecycle contract.
 *
 * @retval KRS_SUCCESS                            Data sent.
 * @retval KRS_ERR_NULL_POINTER                   conn or data is NULL.
 * @retval KRS_ERR_CLIENT_NOT_CONNECTED           conn is not connected.
 * @retval KRS_ERR_SERVER_CONGESTION_WINDOW_FULL  Timeout expired while window remained full.
 * @retval KRS_ERR_MEMORY_ALLOCATION              Frame builder or fragmentation allocation failed.
 * @retval KRS_ERR_BUFFER_TOO_SMALL               Frame serialization failed (single-frame path).
 */
Void_r krs_client_send_blocking(ServerConnection_t* conn, Channel_t channel,
                                const uint8_t* data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief Registers a callback invoked when a reliable message is permanently dropped.
 *
 * Fires once per dropped message after `max_retries` (5) unsuccessful
 * retransmission attempts. The callback is invoked from the client receive
 * thread and MUST NOT call Kronos send APIs synchronously. Pass callback=NULL
 * to clear a previously registered callback.
 *
 * Both `connection_id` and `channel` passed to the callback are accurate:
 * `connection_id` is the ID assigned by the server during the SOCKET_ACK
 * handshake, and `channel` is the channel the dropped message was originally
 * sent on (preserved per-AckEntry).
 *
 * @param conn       The active server connection.
 * @param callback   Callback function pointer (may be NULL to clear).
 * @param user_data  Caller-supplied context passed to the callback.
 */
void krs_client_set_delivery_failure_callback(ServerConnection_t* conn,
                                              DeliveryFailureCallback_f callback, void* user_data);

#endif // KRONOS_CLIENT_H
