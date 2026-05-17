#include "kronos.h"
#include "frame_builder_internal.h"
#include "frame_body.h"

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

void krs_frame_builder_set_fragment_info(FrameBuilder_c* builder, uint16_t index, uint16_t total) {
    if (!builder) return;
    builder->metadata.fragment_index = index;
    builder->metadata.fragment_total = total;
    builder->presence_flags |= (uint16_t)(1u << META_FLAG_FRAGMENT_INFO);
}

void krs_frame_builder_set_ack_id(FrameBuilder_c* builder, uint64_t ack_id) {
    if (!builder) return;
    builder->metadata.ack_id = ack_id;
    builder->presence_flags |= (uint16_t)(1u << META_FLAG_ACK_ID);
}

void krs_frame_builder_set_priority(FrameBuilder_c* builder, uint8_t priority) {
    if (!builder) return;
    builder->metadata.priority = priority;
    builder->presence_flags |= (uint16_t)(1u << META_FLAG_PRIORITY);
}

void krs_frame_builder_set_timestamp(FrameBuilder_c* builder, uint64_t timestamp_ms) {
    if (!builder) return;
    builder->metadata.timestamp_ms = timestamp_ms;
    builder->presence_flags |= (uint16_t)(1u << META_FLAG_TIMESTAMP);
}

uint16_t krs_frame_builder_serialize(FrameBuilder_c* builder, uint8_t* out, uint16_t out_size) {
    if (!builder || !out) return 0;

    uint16_t metadata_size = krs_frame_metadata_block_length(builder->presence_flags);
    uint16_t total = (uint16_t)(KRONOS_FRAME_HEADER_LENGTH + metadata_size + builder->data_length);
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

    uint16_t cursor = KRONOS_FRAME_HEADER_LENGTH;

    if (builder->presence_flags & (uint16_t)(1u << META_FLAG_FRAGMENT_INFO)) {
        out[cursor]     = (uint8_t)(builder->metadata.fragment_index >> 8);
        out[cursor + 1] = (uint8_t)(builder->metadata.fragment_index & 0xFF);
        out[cursor + 2] = (uint8_t)(builder->metadata.fragment_total >> 8);
        out[cursor + 3] = (uint8_t)(builder->metadata.fragment_total & 0xFF);
        cursor += 4;
    }
    if (builder->presence_flags & (uint16_t)(1u << META_FLAG_ACK_ID)) {
        for (int k = 0; k < 8; k++) {
            out[cursor + k] = (uint8_t)((builder->metadata.ack_id >> ((7 - k) * 8)) & 0xFF);
        }
        cursor += 8;
    }
    if (builder->presence_flags & (uint16_t)(1u << META_FLAG_PRIORITY)) {
        out[cursor++] = builder->metadata.priority;
    }
    if (builder->presence_flags & (uint16_t)(1u << META_FLAG_TIMESTAMP)) {
        for (int k = 0; k < 8; k++) {
            out[cursor + k] = (uint8_t)((builder->metadata.timestamp_ms >> ((7 - k) * 8)) & 0xFF);
        }
        cursor += 8;
    }

    if (builder->data && builder->data_length > 0) {
        memcpy(out + cursor, builder->data, builder->data_length);
    }

    return total;
}

void krs_frame_builder_destroy(FrameBuilder_c** builder) {
    if (!builder || !*builder) return;
    free(*builder);
    *builder = NULL;
}
