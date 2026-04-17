#include "kronos_fragment.h"

#include "fragment_internal.h"
#include "frame_metadata.h"
#include "kronos_internal.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>


static uint8_t s_version_byte(void) {
    return krs_version_encode(
        (uint8_t)KRONOS_VERSION_MAJOR,
        (uint8_t)KRONOS_VERSION_MINOR,
        (uint8_t)KRONOS_VERSION_PATCH
    );
}

static void s_write_u16_be(uint8_t* buf, uint16_t value) {
    buf[0] = (uint8_t)((value >> 8) & 0xFF);
    buf[1] = (uint8_t)(value & 0xFF);
}

static void s_write_u64_be(uint8_t* buf, uint64_t value) {
    for (int k = 0; k < 8; k++) {
        buf[k] = (uint8_t)((value >> ((7 - k) * 8)) & 0xFF);
    }
}

static uint8_t* s_build_plain_frame(uint8_t channel, FrameType_e type, uint64_t packet_id,
                                    const uint8_t* data, uint32_t data_length,
                                    uint16_t flags, uint16_t* out_size) {
    *out_size = (uint16_t)(KRONOS_FRAME_HEADER_LENGTH + data_length);
    uint8_t* buf = malloc(*out_size);
    if (!buf) return NULL;

    buf[0] = 0x4B;
    buf[1] = s_version_byte();
    buf[2] = channel;
    buf[3] = (uint8_t)type;
    s_write_u16_be(buf + 4, flags);
    s_write_u64_be(buf + 6, packet_id);
    if (data && data_length > 0) {
        memcpy(buf + KRONOS_FRAME_HEADER_LENGTH, data, data_length);
    }
    return buf;
}

static uint8_t* s_build_fragment_frame(uint8_t channel, FrameType_e type, uint64_t packet_id,
                                       uint16_t frag_index, uint16_t frag_total,
                                       const uint8_t* chunk, uint16_t chunk_size,
                                       uint16_t extra_flags, uint16_t* out_size) {
    *out_size = (uint16_t)(KRONOS_FRAME_HEADER_LENGTH + 4 + chunk_size);
    uint8_t* buf = malloc(*out_size);
    if (!buf) return NULL;

    buf[0] = 0x4B;
    buf[1] = s_version_byte();
    buf[2] = channel;
    buf[3] = (uint8_t)type;
    uint16_t flags = (uint16_t)((1u << META_FLAG_FRAGMENT_INFO) | extra_flags);
    s_write_u16_be(buf + 4, flags);
    s_write_u64_be(buf + 6, packet_id);
    s_write_u16_be(buf + KRONOS_FRAME_HEADER_LENGTH, frag_index);
    s_write_u16_be(buf + KRONOS_FRAME_HEADER_LENGTH + 2, frag_total);
    if (chunk && chunk_size > 0) {
        memcpy(buf + KRONOS_FRAME_HEADER_LENGTH + 4, chunk, chunk_size);
    }
    return buf;
}

FragmentResult_t krs_fragment_split(uint8_t channel, FrameType_e type, uint64_t packet_id,
                                    const uint8_t* data, uint32_t data_length, uint16_t mtu,
                                    uint16_t additional_flags) {
    FragmentResult_t result = {0};

    if (mtu <= KRONOS_FRAME_HEADER_LENGTH) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER,
                                                       "mtu too small for frame header");
        return result;
    }

    uint16_t plain_max = (uint16_t)(mtu - KRONOS_FRAME_HEADER_LENGTH);
    if (data_length <= plain_max) {
        result.fragments = malloc(sizeof(uint8_t*));
        result.fragment_sizes = malloc(sizeof(uint16_t));
        if (!result.fragments || !result.fragment_sizes) {
            free(result.fragments);
            free(result.fragment_sizes);
            result.fragments = NULL;
            result.fragment_sizes = NULL;
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION,
                                                           "allocation failed");
            return result;
        }
        result.fragments[0] = s_build_plain_frame(channel, type, packet_id,
                                                   data, data_length,
                                                   additional_flags,
                                                   &result.fragment_sizes[0]);
        if (!result.fragments[0]) {
            free(result.fragments);
            free(result.fragment_sizes);
            result.fragments = NULL;
            result.fragment_sizes = NULL;
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION,
                                                           "frame allocation failed");
            return result;
        }
        result.fragment_count = 1;
        result._data_pool = NULL;
        result.base = krs_lib_error_result_base_suc();
        return result;
    }

    if (mtu <= KRONOS_FRAME_HEADER_LENGTH + 4) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER,
                                                       "mtu too small for fragment header");
        return result;
    }

    uint16_t payload_per_frag = (uint16_t)(mtu - KRONOS_FRAME_HEADER_LENGTH - 4);
    uint32_t count = (data_length + payload_per_frag - 1) / payload_per_frag;
    if (count > UINT16_MAX) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER,
                                                       "too many fragments needed");
        return result;
    }

    uint16_t frag_count = (uint16_t)count;

    result.fragments = malloc(frag_count * sizeof(uint8_t*));
    result.fragment_sizes = malloc(frag_count * sizeof(uint16_t));
    if (!result.fragments || !result.fragment_sizes) {
        free(result.fragments);
        free(result.fragment_sizes);
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION,
                                                       "allocation failed");
        return result;
    }

    uint32_t total_pool_size = 0;
    uint32_t pre_offset = 0;
    for (uint16_t i = 0; i < frag_count; i++) {
        uint32_t remaining = data_length - pre_offset;
        uint16_t chunk_size = (remaining > payload_per_frag) ? payload_per_frag : (uint16_t)remaining;
        total_pool_size += KRONOS_FRAME_HEADER_LENGTH + 4 + chunk_size;
        pre_offset += chunk_size;
    }

    uint8_t* pool = malloc(total_pool_size);
    if (!pool) {
        free(result.fragments);
        free(result.fragment_sizes);
        result.fragments = NULL;
        result.fragment_sizes = NULL;
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION,
                                                       "pool allocation failed");
        return result;
    }

    result._data_pool = pool;
    uint32_t pool_offset = 0;
    uint32_t data_offset = 0;
    uint8_t ver = s_version_byte();

    for (uint16_t i = 0; i < frag_count; i++) {
        uint32_t remaining = data_length - data_offset;
        uint16_t chunk_size = (remaining > payload_per_frag) ? payload_per_frag : (uint16_t)remaining;
        uint16_t frame_size = (uint16_t)(KRONOS_FRAME_HEADER_LENGTH + 4 + chunk_size);

        uint8_t* buf = pool + pool_offset;
        result.fragments[i] = buf;
        result.fragment_sizes[i] = frame_size;

        buf[0] = 0x4B;
        buf[1] = ver;
        buf[2] = channel;
        buf[3] = (uint8_t)type;
        uint16_t flags = (uint16_t)((1u << META_FLAG_FRAGMENT_INFO) | additional_flags);
        s_write_u16_be(buf + 4, flags);
        s_write_u64_be(buf + 6, packet_id);
        s_write_u16_be(buf + KRONOS_FRAME_HEADER_LENGTH, i);
        s_write_u16_be(buf + KRONOS_FRAME_HEADER_LENGTH + 2, frag_count);
        if (data && chunk_size > 0) {
            memcpy(buf + KRONOS_FRAME_HEADER_LENGTH + 4, data + data_offset, chunk_size);
        }

        pool_offset += frame_size;
        data_offset += chunk_size;
    }

    result.fragment_count = frag_count;
    result.base = krs_lib_error_result_base_suc();
    return result;
}

void krs_fragment_result_destroy(FragmentResult_t* result) {
    if (!result) return;
    if (result->_data_pool) {
        free(result->_data_pool);
    } else {
        for (uint16_t i = 0; i < result->fragment_count; i++) {
            free(result->fragments[i]);
        }
    }
    free(result->fragments);
    free(result->fragment_sizes);
    result->fragments = NULL;
    result->fragment_sizes = NULL;
    result->_data_pool = NULL;
    result->fragment_count = 0;
}

static void s_session_destroy(void* item) {
    FragmentSession_t* session = (FragmentSession_t*)item;
    if (!session) return;
    free(session->buffer);
    free(session->piece_sizes);
    free(session->piece_received);
    free(session);
}

Reassembler_t* krs_reassembler_create(void) {
    Reassembler_t* r = malloc(sizeof(Reassembler_t));
    if (!r) return NULL;
    r->sessions = krs_array_create(4);
    if (!r->sessions) {
        free(r);
        return NULL;
    }
    return r;
}

void krs_reassembler_destroy(Reassembler_t** reassembler) {
    if (!reassembler || !*reassembler) return;
    krs_array_destroy_items(&(*reassembler)->sessions, s_session_destroy);
    free(*reassembler);
    *reassembler = NULL;
}

uint32_t krs_reassembler_sweep_stale(Reassembler_t* reassembler, uint32_t timeout_ms) {
    if (!reassembler || !reassembler->sessions) return 0;

    uint64_t now = GetTickCount64();
    uint32_t removed = 0;
    uint32_t i = krs_array_length(reassembler->sessions);

    while (i > 0) {
        i--;
        FragmentSession_t* session = KRS_ARRAY_GET(reassembler->sessions, i, FragmentSession_t);
        if (!session) continue;
        if (now - session->created_ms < timeout_ms) continue;
        s_session_destroy(session);
        krs_array_remove(reassembler->sessions, i);
        removed++;
    }

    return removed;
}

static FragmentSession_t* s_find_session(Reassembler_t* r, uint64_t packet_id) {
    uint32_t len = krs_array_length(r->sessions);
    for (uint32_t i = 0; i < len; i++) {
        FragmentSession_t* s = KRS_ARRAY_GET(r->sessions, i, FragmentSession_t);
        if (s && s->packet_id == packet_id) return s;
    }
    return NULL;
}

static FragmentSession_t* s_create_session(Reassembler_t* r, uint64_t packet_id, uint16_t total) {
    FragmentSession_t* session = malloc(sizeof(FragmentSession_t));
    if (!session) return NULL;

    session->packet_id = packet_id;
    session->total = total;
    session->received = 0;
    session->max_piece_size = KRS_MAX_PAYLOAD_PER_FRAGMENT;
    session->ack_required = false;
    session->created_ms = GetTickCount64();

    session->buffer = malloc((uint32_t)total * session->max_piece_size);
    session->piece_sizes = calloc(total, sizeof(uint16_t));
    session->piece_received = calloc(total, sizeof(bool));

    if (!session->buffer || !session->piece_sizes || !session->piece_received) {
        free(session->buffer);
        free(session->piece_sizes);
        free(session->piece_received);
        free(session);
        return NULL;
    }

    if (krs_array_push(r->sessions, session).base.error_code != KRS_SUCCESS) {
        free(session->buffer);
        free(session->piece_sizes);
        free(session->piece_received);
        free(session);
        return NULL;
    }
    return session;
}

static void s_remove_session(Reassembler_t* r, uint64_t packet_id) {
    uint32_t len = krs_array_length(r->sessions);
    for (uint32_t i = 0; i < len; i++) {
        FragmentSession_t* s = KRS_ARRAY_GET(r->sessions, i, FragmentSession_t);
        if (s && s->packet_id == packet_id) {
            krs_array_remove(r->sessions, i);
            return;
        }
    }
}

ReassembleResult_t krs_reassembler_feed(Reassembler_t* reassembler, const Frame_t* fragment) {
    ReassembleResult_t result = {0};

    if (!reassembler || !fragment) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "NULL argument");
        return result;
    }

    bool has_fragment_info = (fragment->presence_flags & (uint16_t)(1u << META_FLAG_FRAGMENT_INFO)) != 0;

    if (!has_fragment_info) {
        result.data = malloc(fragment->body_length);
        if (!result.data) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION,
                                                           "allocation failed");
            return result;
        }
        if (fragment->body_length > 0) {
            memcpy(result.data, fragment->body, fragment->body_length);
        }
        result.data_length = fragment->body_length;
        result.complete = true;
        result.ack_required = (fragment->presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) != 0;
        result.base = krs_lib_error_result_base_suc();
        return result;
    }

    if (fragment->body_length < 4) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER,
                                                       "fragment body too small");
        return result;
    }

    uint16_t index = ((uint16_t)fragment->body[0] << 8) | fragment->body[1];
    uint16_t total = ((uint16_t)fragment->body[2] << 8) | fragment->body[3];
    uint16_t payload_size = (uint16_t)(fragment->body_length - 4);
    const uint8_t* payload = fragment->body + 4;

    if (total == 0 || index >= total) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER,
                                                       "invalid fragment index/total");
        return result;
    }

    FragmentSession_t* session = s_find_session(reassembler, fragment->packet_id);
    if (!session) {
        session = s_create_session(reassembler, fragment->packet_id, total);
        if (!session) {
            result.base = krs_lib_error_result_base_w_msg(KRS_ERR_MEMORY_ALLOCATION,
                                                           "session allocation failed");
            return result;
        }
    }

    if (fragment->presence_flags & (uint16_t)(1u << META_FLAG_ACK_REQUIRED)) {
        session->ack_required = true;
    }

    if (session->piece_received[index]) {
        result.base = krs_lib_error_result_base_suc();
        return result;
    }

    if (payload_size > 0) {
        memcpy(session->buffer + ((uint32_t)index * session->max_piece_size), payload, payload_size);
    }
    session->piece_sizes[index] = payload_size;
    session->piece_received[index] = true;
    session->received++;

    if (session->received < session->total) {
        result.base = krs_lib_error_result_base_suc();
        result.complete = false;
        return result;
    }

    uint32_t total_length = 0;
    for (uint16_t i = 0; i < session->total; i++) {
        total_length += session->piece_sizes[i];
    }

    uint32_t offset = 0;
    for (uint16_t i = 0; i < session->total; i++) {
        uint32_t slot_offset = (uint32_t)i * session->max_piece_size;
        if (session->piece_sizes[i] > 0 && offset != slot_offset) {
            memmove(session->buffer + offset,
                    session->buffer + slot_offset,
                    session->piece_sizes[i]);
        }
        offset += session->piece_sizes[i];
    }

    result.data = session->buffer;
    session->buffer = NULL;
    result.ack_required = session->ack_required;

    s_remove_session(reassembler, fragment->packet_id);
    s_session_destroy(session);

    result.data_length = total_length;
    result.complete = true;
    result.base = krs_lib_error_result_base_suc();
    return result;
}
