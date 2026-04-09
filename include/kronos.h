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
 * KRONOS_FRAME_HEADER_LENGTH, or the first byte is not 0x4B.
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
 * @param buffer              Raw UDP datagram bytes.
 * @param received_bytes      Number of bytes received.
 * @param stack_data_out      Caller-allocated buffer for frame body.
 * @param stack_data_out_size Size of stack_data_out.
 * @return FrameCreate_r containing the frame or error information.
 *
 * @retval KRS_SUCCESS                  Frame parsed successfully.
 * @retval KRS_ERR_NULL_POINTER         buffer or stack_data_out is NULL.
 * @retval KRS_ERR_FRAME_INVALID_HEADER received_bytes < KRONOS_FRAME_HEADER_LENGTH.
 * @retval KRS_ERR_FRAME_INVALID_PROTOCOL First byte is not 0x4B.
 * @retval KRS_ERR_BUFFER_TOO_SMALL     stack_data_out_size too small for body.
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
 * @return Pointer to the new FrameBuilder_t, or NULL on allocation failure.
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
 * @brief Serializes the frame builder into a byte buffer in Kronos wire format.
 *
 * Wire format: [0]='K' [1]=version [2]=channel [3]=type [4-5]=presence_flags(BE)
 *              [6-13]=packet_id(BE) [14..]=body.
 *
 * @param builder   The frame builder.
 * @param out       Destination buffer.
 * @param out_size  Capacity of out in bytes.
 * @return Total bytes written, or 0 if out is NULL/too small.
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
    MESSAGE_ACK   = 0,
    BASIC_MESSAGE = 1,

    CONNECTION   = 10,
    HEARTBEAT    = 11,
    SOCKET_SETUP = 12,

    SOCKET_ACK = 22,
};

struct FrameCreateResult {
    KronosResult_b base;
    Frame_t frame;
};

#endif // KRONOS_H
