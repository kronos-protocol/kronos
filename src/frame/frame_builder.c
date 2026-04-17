#include "kronos.h"
#include "frame_builder_internal.h"

#include <stdlib.h>
#include <string.h>


FrameBuilder_c* krs_frame_builder_create(uint8_t channel, FrameType_e type) {
    FrameBuilder_c* builder = calloc(1, sizeof(FrameBuilder_c));
    if (!builder) return NULL;
    builder->channel = channel;
    builder->type = type;
    return builder;
}

void krs_frame_builder_set_packet_id(FrameBuilder_c* builder, uint64_t packet_id) {
    if (!builder) return;
    builder->packet_id = packet_id;
}

void krs_frame_builder_set_flag(FrameBuilder_c* builder, MetadataFlagPosition_e flag) {
    if (!builder || flag >= META_FLAG_COUNT) return;
    builder->presence_flags |= (uint16_t)(1u << flag);
}

void krs_frame_builder_set_data(FrameBuilder_c* builder, const uint8_t* data, uint16_t length) {
    if (!builder) return;
    builder->data = data;
    builder->data_length = length;
}

uint16_t krs_frame_builder_serialize(FrameBuilder_c* builder, uint8_t* out, uint16_t out_size) {
    if (!builder || !out) return 0;

    uint16_t total = KRONOS_FRAME_HEADER_LENGTH + builder->data_length;
    if (out_size < total) return 0;

    out[0] = 0x4B;
    out[1] = krs_version_encode(KRONOS_VERSION_MAJOR, KRONOS_VERSION_MINOR, KRONOS_VERSION_PATCH);
    out[2] = builder->channel;
    out[3] = (uint8_t)builder->type;
    out[4] = (uint8_t)(builder->presence_flags >> 8);
    out[5] = (uint8_t)(builder->presence_flags & 0xFF);
    out[6]  = (uint8_t)(builder->packet_id >> 56);
    out[7]  = (uint8_t)(builder->packet_id >> 48);
    out[8]  = (uint8_t)(builder->packet_id >> 40);
    out[9]  = (uint8_t)(builder->packet_id >> 32);
    out[10] = (uint8_t)(builder->packet_id >> 24);
    out[11] = (uint8_t)(builder->packet_id >> 16);
    out[12] = (uint8_t)(builder->packet_id >> 8);
    out[13] = (uint8_t)(builder->packet_id & 0xFF);

    if (builder->data && builder->data_length > 0) {
        memcpy(out + KRONOS_FRAME_HEADER_LENGTH, builder->data, builder->data_length);
    }

    return total;
}

void krs_frame_builder_destroy(FrameBuilder_c** builder) {
    if (!builder || !*builder) return;
    free(*builder);
    *builder = NULL;
}
