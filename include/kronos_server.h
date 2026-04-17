#ifndef KRONOS_SERVER_H
#define KRONOS_SERVER_H
#include "kronos_network.h"
#include "kronos_stats.h"
#include <stdbool.h>

/** @brief Opaque server port manager owning a port table and default configuration. */
typedef struct ServerPortManager ServerPortManager_t;

/** @brief Opaque per-port descriptor holding a socket, channel types, and protocol state. */
typedef struct UDPSocketDescriptor UDPSocketDescriptor_t;

/** @brief Opaque client connection object (server-side, represents one connected client). */
typedef struct ClientConnection ClientConnection_t;

/**
 * @brief Callback invoked when a client connects or disconnects.
 *
 * @param connection_id  The connection ID of the connecting/disconnecting client.
 * @param channel        The channel on which the connection event occurred.
 * @param user_data      User-supplied context pointer.
 */
typedef void (*ConnectionLifecycleCallback_f)(uint32_t connection_id, Channel_t channel, void* user_data);

/**
 * @brief Callback invoked when a message arrives on a port or channel.
 *
 * @param channel        Channel on which the message arrived.
 * @param channel_type   Operating mode of the channel.
 * @param connection_id  Identifier of the sending connection (0 if unknown).
 * @param data           Pointer to the message payload bytes.
 * @param data_length    Number of payload bytes.
 * @param user_data      User-supplied context pointer registered with the callback.
 */
typedef void (*ChannelMessageCallback_f)(Channel_t channel,
                                         ChannelType_e channel_type,
                                         uint32_t connection_id,
                                         const uint8_t* data,
                                         uint16_t data_length,
                                         void* user_data);

/**
 * @brief Creates a new server port manager bound to a default address.
 *
 * @param default_address  The address all subsequently added ports will bind to by default.
 * @return Pointer to the new ServerPortManager_t, or NULL on allocation failure.
 */
ServerPortManager_t* krs_server_port_manager_create(Address_t default_address);

/**
 * @brief Destroys a server port manager and all its registered descriptors.
 *
 * @param spm  Pointer to the port manager pointer; set to NULL on return.
 */
void krs_server_port_manager_destroy(ServerPortManager_t** spm);

/**
 * @brief Adds a port to the server using the manager's default address.
 *
 * Creates a UDPSocketDescriptor_t, opens and binds a UDP socket, and registers
 * the descriptor in the port table.
 *
 * @param spm   The server port manager.
 * @param port  The port number to add.
 */
void krs_server_port_manager_port_add(ServerPortManager_t* spm, Port_t port);

/**
 * @brief Adds a port to the server using an explicit address.
 *
 * @param spm      The server port manager.
 * @param port     The port number to add.
 * @param address  The address to bind this port to.
 */
void krs_server_port_manager_port_add_with_address(ServerPortManager_t* spm, Port_t port, Address_t address);

/**
 * @brief Creates a UDP socket descriptor for the given port address.
 *
 * Allocates a UDPSocketDescriptor_t, opens a UDP socket, and binds it.
 *
 * @param port_address  The address and port to bind to.
 * @return Pointer to the new UDPSocketDescriptor_t, or NULL on failure.
 */
UDPSocketDescriptor_t* krs_server_udp_socket_handler_create(PortAddress_t port_address);

/**
 * @brief Destroys a UDP socket descriptor and closes its socket.
 *
 * @param socket_handler  Pointer to the descriptor pointer; set to NULL on return.
 */
void krs_server_udp_socket_handler_destroy(UDPSocketDescriptor_t** socket_handler);

/**
 * @brief Registers a port-level callback invoked for any channel without a specific callback.
 *
 * @param spm       The server port manager.
 * @param port      The port whose descriptor receives the callback.
 * @param callback  Callback function pointer.
 * @param user_data Caller-supplied context passed to the callback.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS           Callback registered.
 * @retval KRS_ERR_NULL_POINTER  spm is NULL.
 * @retval KRS_ERR_NOT_INITIALIZED  No descriptor found for the given port.
 */
Void_r krs_server_set_port_callback(ServerPortManager_t* spm, Port_t port,
                                    ChannelMessageCallback_f callback, void* user_data);

/**
 * @brief Registers a channel-specific callback for an application channel.
 *
 * Channels 0–9 are reserved and will cause this function to return an error.
 *
 * @param spm       The server port manager.
 * @param port      The port whose descriptor receives the callback.
 * @param channel   The channel to bind the callback to (must be >= 10).
 * @param callback  Callback function pointer.
 * @param user_data Caller-supplied context passed to the callback.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                  Callback registered.
 * @retval KRS_ERR_NULL_POINTER         spm is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER    channel < 10 (reserved range).
 * @retval KRS_ERR_NOT_INITIALIZED      No descriptor found for the given port.
 */
Void_r krs_server_set_channel_callback(ServerPortManager_t* spm, Port_t port, Channel_t channel,
                                       ChannelMessageCallback_f callback, void* user_data);

/**
 * @brief Sends data to a specific connected client by connection ID.
 *
 * Looks up the client's remote address from the connection table and sends
 * the payload. Uses fragmentation if the data exceeds MTU.
 *
 * @param spm            The server port manager.
 * @param connection_id  Target client connection ID.
 * @param channel        Target channel.
 * @param data           Payload bytes to send.
 * @param length         Number of payload bytes.
 * @param require_ack    If true, registers the packet for ACK tracking.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                  Data sent.
 * @retval KRS_ERR_NULL_POINTER         spm or data is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER    connection_id not found.
 * @retval KRS_ERR_NETWORK_SOCKET_ERROR Send failed.
 */
Void_r krs_server_send(ServerPortManager_t* spm, uint32_t connection_id, Channel_t channel,
                       const uint8_t* data, uint16_t length, bool require_ack);

/**
 * @brief Broadcasts data to all connections on the given channel across all registered ports.
 *
 * @param spm     The server port manager.
 * @param channel Target channel.
 * @param data    Payload bytes to broadcast.
 * @param length  Number of payload bytes.
 */
void krs_server_broadcast(ServerPortManager_t* spm, Channel_t channel,
                          const uint8_t* data, uint16_t length);

/**
 * @brief Broadcasts data to all connections on the given channel, excluding one.
 *
 * @param spm                   The server port manager.
 * @param channel               Target channel.
 * @param exclude_connection_id Connection ID to skip.
 * @param data                  Payload bytes to broadcast.
 * @param length                Number of payload bytes.
 */
void krs_server_broadcast_except(ServerPortManager_t* spm, Channel_t channel,
                                 uint32_t exclude_connection_id,
                                 const uint8_t* data, uint16_t length);

/**
 * @brief Starts the IOCP I/O and message handler threads for the server.
 *
 * Calls WSAStartup, creates the IOCP handle, associates all registered sockets,
 * posts initial WSARecvFrom operations, and spawns worker threads.
 *
 * @param spm  The server port manager.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                   Server started successfully.
 * @retval KRS_ERR_NULL_POINTER          spm is NULL.
 * @retval KRS_ERR_SERVER_ALREADY_RUNNING  Server is already running.
 * @retval KRS_ERR_PLATFORM_WINDOWS_SOCKET  IOCP or WSA initialization failed.
 * @retval KRS_ERR_MEMORY_ALLOCATION     Thread array allocation failed.
 */
Void_r krs_server_start(ServerPortManager_t* spm);

/**
 * @brief Stops the server, signals all threads to exit, and frees IOCP resources.
 *
 * @param spm  The server port manager.
 */
void krs_server_stop(ServerPortManager_t* spm);

/**
 * @brief Registers a callback invoked when a new client connects on any channel.
 *
 * @param spm        The server port manager.
 * @param port       The port whose descriptor receives the callback.
 * @param callback   Callback function pointer.
 * @param user_data  Caller-supplied context.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS           Callback registered.
 * @retval KRS_ERR_NULL_POINTER  spm is NULL.
 * @retval KRS_ERR_NOT_INITIALIZED  No descriptor for port.
 */
Void_r krs_server_set_connect_callback(ServerPortManager_t* spm, Port_t port,
                                       ConnectionLifecycleCallback_f callback, void* user_data);

/**
 * @brief Registers a callback invoked when a client is disconnected (heartbeat timeout).
 *
 * @param spm        The server port manager.
 * @param port       The port whose descriptor receives the callback.
 * @param callback   Callback function pointer.
 * @param user_data  Caller-supplied context.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS           Callback registered.
 * @retval KRS_ERR_NULL_POINTER  spm is NULL.
 * @retval KRS_ERR_NOT_INITIALIZED  No descriptor for port.
 */
Void_r krs_server_set_disconnect_callback(ServerPortManager_t* spm, Port_t port,
                                          ConnectionLifecycleCallback_f callback, void* user_data);

/**
 * @brief Configures the number of IOCP I/O threads and message handler threads.
 *
 * Must be called before krs_server_start(). If not called, defaults are
 * 2 IOCP threads and 4 handler threads.
 *
 * @param spm              The server port manager.
 * @param iocp_threads     Number of IOCP I/O threads (minimum 1).
 * @param handler_threads  Number of message handler threads (minimum 1).
 */
void krs_server_set_thread_counts(ServerPortManager_t* spm,
                                  uint32_t iocp_threads, uint32_t handler_threads);

/**
 * @brief Retrieves a snapshot of current server runtime statistics.
 *
 * All counters are read atomically but the snapshot is not globally
 * consistent — individual fields may reflect slightly different points
 * in time under concurrent load.
 *
 * @param spm  The server port manager.
 * @return ServerStats_t with current counter values. Returns zeroed struct if spm is NULL.
 */
ServerStats_t krs_server_get_stats(const ServerPortManager_t* spm);

/**
 * @brief Sends data reliably to a client with automatic congestion backpressure retry.
 *
 * Calls krs_server_send with require_ack=true. If the congestion window is
 * full, retries with 1ms sleep intervals until success or timeout_ms expires.
 *
 * @param spm            The server port manager.
 * @param connection_id  Target client connection ID.
 * @param channel        Target channel.
 * @param data           Payload bytes to send.
 * @param length         Number of payload bytes.
 * @param timeout_ms     Maximum time to wait in milliseconds. Pass 0 for single attempt.
 * @return Void_r indicating success or failure.
 *
 * @retval KRS_SUCCESS                          Data sent.
 * @retval KRS_ERR_NULL_POINTER                 spm or data is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER            connection_id not found.
 * @retval KRS_ERR_SERVER_CONGESTION_WINDOW_FULL  Timeout expired while window full.
 */
Void_r krs_server_send_blocking(ServerPortManager_t* spm, uint32_t connection_id,
                                Channel_t channel, const uint8_t* data,
                                uint16_t length, uint32_t timeout_ms);

/**
 * @brief Registers the same callback for all channels in a range [from_channel, to_channel].
 *
 * Both from_channel and to_channel are inclusive. Both must be >= 10 (reserved
 * channels 0–9 are rejected). Equivalent to calling krs_server_set_channel_callback
 * for each channel in the range.
 *
 * @param spm          The server port manager.
 * @param port         The port whose descriptor receives the callbacks.
 * @param from_channel First channel in the range (inclusive, must be >= 10).
 * @param to_channel   Last channel in the range (inclusive, must be >= from_channel).
 * @param callback     Callback function pointer.
 * @param user_data    Caller-supplied context passed to the callback.
 * @return Void_r indicating success or failure. Returns error on first failing channel.
 *
 * @retval KRS_SUCCESS                  All callbacks registered.
 * @retval KRS_ERR_NULL_POINTER         spm is NULL.
 * @retval KRS_ERR_INVALID_PARAMETER    from_channel < 10, to_channel < from_channel, or channel > 255.
 * @retval KRS_ERR_NOT_INITIALIZED      No descriptor found for the given port.
 */
Void_r krs_server_set_channel_range_callback(ServerPortManager_t* spm, Port_t port,
                                             Channel_t from_channel, Channel_t to_channel,
                                             ChannelMessageCallback_f callback, void* user_data);

#endif // KRONOS_SERVER_H
