#ifndef KRONOS_H
#define KRONOS_H
#include <kronos_internal.h>
#include <kronos_error.h>
#include <frame_metadata.h>
#include <stdint.h>

/**
 * DISCLAIMER
 * General Infos about the Kronos library:
 * The library is fully functional by just using the headers in the `include` directory.
 * If you want more control you can use the internal headers. Use at your own discretion.
 *
 * The following typedef suffixes have a meaning:
 * _e: Enums.
 * _t: Structs which are used as plain data models.
 * _c: Structs which serve a purpose, more in line with the OOP model of a class.
 *     These structs usually have functions serving as methods.
 * _f: Function pointers.
 */

/** @brief Opaque frame type. Full definition in kronos_internal.h. */
typedef struct Frame Frame_t;

/** @brief Frame type identifier. */
typedef enum FrameType FrameType_e;

/** @brief Opaque frame builder for constructing serialized frames. */
typedef struct FrameBuilder FrameBuilder_c;

/** @brief Result type for safe frame creation. */
typedef struct FrameCreateResult FrameCreate_r;

/**
 * @brief Parses a raw UDP datagram into a frame using caller-provided stack storage for the body.
 *
 * Returns a zero-initialized Frame_t if buffer is NULL, received_bytes is less than
 * KRONOS_FRAME_HEADER_LENGTH, the first byte is not 0x4B, or the encoded major
 * version does not match the build's KRONOS_VERSION_MAJOR.
 *
 * @param buffer              Raw UDP datagram bytes.
 * @param received_bytes      Number of bytes received.
 * @param stack_data_out      Caller-allocated buffer to hold the frame body.
 * @param stack_data_out_size Size of stack_data_out in bytes.
 * @return Parsed Frame_t. Returns a zero-initialized Frame_t on error.
 */
Frame_t krs_frame_create(const uint8_t* buffer, uint16_t received_bytes, uint8_t* stack_data_out, uint16_t stack_data_out_size);

/**
 * @brief Parses a raw UDP datagram into a frame with explicit error handling.
 *
 * Validates the frame magic byte AND the encoded major version. Frames
 * with a different major version than the build's KRONOS_VERSION_MAJOR
 * are rejected. Minor and patch differences are accepted (semver-style
 * compatibility within a major release).
 *
 * @param buffer              Raw UDP datagram bytes.
 * @param received_bytes      Number of bytes received.
 * @param stack_data_out      Caller-allocated buffer for frame body.
 * @param stack_data_out_size Size of stack_data_out.
 * @return FrameCreate_r containing the frame or error information.
 *
 * @retval KRS_SUCCESS                       Frame parsed successfully.
 * @retval KRS_ERR_NULL_POINTER              buffer or stack_data_out is NULL.
 * @retval KRS_ERR_FRAME_INVALID_HEADER      received_bytes < KRONOS_FRAME_HEADER_LENGTH.
 * @retval KRS_ERR_FRAME_INVALID_PROTOCOL    First byte is not 0x4B.
 * @retval KRS_ERR_FRAME_UNSUPPORTED_VERSION Major version does not match build.
 * @retval KRS_ERR_BUFFER_TOO_SMALL          stack_data_out_size too small for body.
 */
FrameCreate_r krs_frame_create_s(const uint8_t* buffer, uint16_t received_bytes,
                                 uint8_t* stack_data_out, uint16_t stack_data_out_size);

/**
 * @brief Parses a raw UDP datagram into a heap-allocated frame.
 *
 * The caller is responsible for calling krs_frame_destroy() on the returned frame.
 *
 * @param buffer          Raw UDP datagram bytes.
 * @param received_bytes  Number of bytes received.
 * @return Pointer to a heap-allocated Frame_t, or NULL on allocation failure or invalid frame.
 */
Frame_t* krs_frame_create_heap(const uint8_t* buffer, uint16_t received_bytes);

/**
 * @brief Parses a raw UDP datagram into a pre-allocated Frame_t in place.
 *
 * Include the kronos_internal.h to use this effectively.
 *
 * @param buffer          Raw UDP datagram bytes.
 * @param received_bytes  Number of bytes received.
 * @param out             Pre-allocated Frame_t to write into.
 * @param out_data_size   Capacity of out->body.
 */
void krs_frame_init(const uint8_t* buffer, uint16_t received_bytes, Frame_t* out, uint16_t out_data_size);

/**
 * @brief Returns the body length of a frame given the total received byte count.
 *
 * @param received_bytes  Total number of bytes received in the datagram.
 * @return Number of bytes in the frame body (received_bytes - KRONOS_FRAME_HEADER_LENGTH).
 */
uint16_t krs_frame_calculate_body_length(uint16_t received_bytes);

/**
 * @brief Copies the frame body into the provided output buffer.
 *
 * @param frame         The frame whose body to copy.
 * @param out           Destination buffer.
 * @param out_data_size Capacity of out in bytes.
 * @return Number of bytes written to out.
 */
uint16_t krs_frame_get_content(const Frame_t* frame, uint8_t* out, uint16_t out_data_size);

/**
 * @brief Frees a heap-allocated frame and its body.
 *
 * Sets *frame to NULL after freeing.
 *
 * @param frame  Pointer to the frame pointer to destroy.
 */
void krs_frame_destroy(Frame_t** frame);

/**
 * @brief Creates a new frame builder for the given channel and frame type.
 *
 * @param channel  Channel number (0–255).
 * @param type     Frame type identifier.
 * @return Pointer to the new FrameBuilder_c, or NULL on allocation failure.
 */
FrameBuilder_c* krs_frame_builder_create(uint8_t channel, FrameType_e type);

/**
 * @brief Sets the packet ID on the frame builder.
 *
 * @param builder    The frame builder.
 * @param packet_id  64-bit packet identifier.
 */
void krs_frame_builder_set_packet_id(FrameBuilder_c* builder, uint64_t packet_id);

/**
 * @brief Sets a presence flag bit on the frame builder.
 *
 * @note For META_FLAG_ACK_REQUIRED (presence-only, no payload) this is the
 *       sufficient call. For META_FLAG_FRAGMENT_INFO, META_FLAG_ACK_ID,
 *       META_FLAG_PRIORITY, and META_FLAG_TIMESTAMP, prefer the dedicated
 *       setters (krs_frame_builder_set_fragment_info etc.); they set the
 *       flag bit AND populate the value that will be serialized. Calling
 *       this function alone for those flags will serialize a metadata block
 *       with default-zero values, which is wire-correct but semantically
 *       meaningless.
 *
 * @param builder  The frame builder.
 * @param flag     The metadata flag position to set.
 */
void krs_frame_builder_set_flag(FrameBuilder_c* builder, MetadataFlagPosition_e flag);

/**
 * @brief Attaches a body payload to the frame builder.
 *
 * The data pointer is not copied; the caller must keep it valid until serialize is called.
 *
 * @param builder  The frame builder.
 * @param data     Pointer to the payload bytes.
 * @param length   Number of payload bytes.
 */
void krs_frame_builder_set_data(FrameBuilder_c* builder, const uint8_t* data, uint16_t length);

/**
 * @brief Sets the FRAGMENT_INFO metadata and flips the corresponding presence bit.
 *
 * The serialized frame will carry a 4-byte fragment header (index:u16,
 * total:u16, big-endian) at the start of the body, before any application
 * payload set via krs_frame_builder_set_data.
 *
 * @param builder  The frame builder.
 * @param index    Fragment index (0-based, must be < total).
 * @param total    Total number of fragments for the original packet (>= 1).
 */
void krs_frame_builder_set_fragment_info(FrameBuilder_c* builder, uint16_t index, uint16_t total);

/**
 * @brief Sets the ACK_ID metadata and flips the corresponding presence bit.
 *
 * Used when the frame itself is acknowledging a specific packet ID inline,
 * distinct from the receiver-side MESSAGE_ACK frame mechanism.
 *
 * @param builder  The frame builder.
 * @param ack_id   The 64-bit packet ID being acknowledged.
 */
void krs_frame_builder_set_ack_id(FrameBuilder_c* builder, uint64_t ack_id);

/**
 * @brief Sets the PRIORITY metadata and flips the corresponding presence bit.
 *
 * @param builder   The frame builder.
 * @param priority  Application-defined priority value (0-255).
 */
void krs_frame_builder_set_priority(FrameBuilder_c* builder, uint8_t priority);

/**
 * @brief Sets the TIMESTAMP metadata and flips the corresponding presence bit.
 *
 * @param builder        The frame builder.
 * @param timestamp_ms   Milliseconds since epoch (or any monotonic clock,
 *                       interpretation is application-defined).
 */
void krs_frame_builder_set_timestamp(FrameBuilder_c* builder, uint64_t timestamp_ms);

/**
 * @brief Serializes the frame builder into a byte buffer in Kronos wire format.
 *
 * Wire format:
 *   [0]='K' [1]=version [2]=channel [3]=type [4-5]=presence_flags(BE)
 *   [6-13]=packet_id(BE) [14..14+M-1]=metadata block [14+M..]=body
 *
 * The metadata block is M bytes long, where M is the sum of
 * KRS_METADATA_FLAG_POSITION_SIZE[i] for each presence flag bit i set.
 * Within the block, payloads are written in bit-position order.
 *
 * @param builder   The frame builder.
 * @param out       Destination buffer.
 * @param out_size  Capacity of out in bytes.
 * @return Total bytes written, or 0 if out is NULL or too small.
 */
uint16_t krs_frame_builder_serialize(FrameBuilder_c* builder, uint8_t* out, uint16_t out_size);

/**
 * @brief Destroys a frame builder, freeing its memory.
 *
 * @param builder  Pointer to the builder pointer; set to NULL on return.
 */
void krs_frame_builder_destroy(FrameBuilder_c** builder);

/**
 * @brief Encodes major, minor, and patch version components into a single byte.
 *
 * Encoding: major(3 bits) | minor(3 bits) | patch(2 bits).
 *
 * @param major  Major version (0–7).
 * @param minor  Minor version (0–7).
 * @param patch  Patch version (0–3).
 * @return Encoded version byte.
 */
uint8_t krs_version_encode(uint8_t major, uint8_t minor, uint8_t patch);

/**
 * @brief Decodes a version byte into its major, minor, and patch components.
 *
 * @param version  Encoded version byte.
 * @param major    Output for major version (may be NULL).
 * @param minor    Output for minor version (may be NULL).
 * @param patch    Output for patch version (may be NULL).
 */
void krs_version_decode(uint8_t version, uint8_t* major, uint8_t* minor, uint8_t* patch);

/**
 * @brief Frame type codes carried in the frame header.
 *
 * Values 0–9 are general purpose. 10–19 are client-only. 20–29 are server-only.
 */
enum FrameType {
    /** @brief Acknowledgement of a packet sent with META_FLAG_ACK_REQUIRED. */
    MESSAGE_ACK   = 0,

    /** @brief Application-level data frame. */
    BASIC_MESSAGE = 1,

    /** @brief Client-to-server handshake on channel 0. */
    CONNECTION   = 10,

    /** @brief Periodic client liveness signal on channel 1. */
    HEARTBEAT    = 11,

    /** @brief Graceful client disconnect on channel 0. */
    DISCONNECT    = 13,

    /**
     * @brief Client-to-server channel subscription request on channel 0.
     *
     * Body is exactly 1 byte: the target application channel (must be >= 10).
     * Recommended: send with META_FLAG_ACK_REQUIRED. The server runs the
     * subscription before sending the MESSAGE_ACK, so receipt of the ACK
     * guarantees the subscription is committed and the client may safely
     * begin sending on the target channel.
     */
    SUBSCRIBE     = 14,

    /**
     * @brief Client-to-server channel unsubscribe request on channel 0.
     *
     * Body is exactly 1 byte: the target application channel. After the
     * server processes this frame, the client will no longer receive
     * application messages on that channel. Idempotent: unsubscribing
     * from a non-subscribed channel is a no-op.
     */
    UNSUBSCRIBE   = 15,

    /** @brief Server reply to CONNECTION carrying the assigned 32-bit connection ID. */
    SOCKET_ACK = 22,
};

struct FrameCreateResult {
    KronosResult_b base;
    Frame_t frame;
};

#endif // KRONOS_H
