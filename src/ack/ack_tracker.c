#include "kronos_ack.h"
#include "kronos_array.h"

#include "ack_internal.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>


static void s_entry_destroy(void* item) {
    AckEntry_t* entry = (AckEntry_t*)item;
    if (!entry) return;
    free(entry->frame_data);
    free(entry);
}

AckTracker_t* krs_ack_tracker_create(uint32_t timeout_ms, uint8_t max_retries) {
    AckTracker_t* tracker = malloc(sizeof(AckTracker_t));
    if (!tracker) return NULL;

    tracker->pending = krs_array_create(8);
    if (!tracker->pending) {
        free(tracker);
        return NULL;
    }

    tracker->timeout_ms = timeout_ms;
    tracker->max_retries = max_retries;
    return tracker;
}

void krs_ack_tracker_destroy(AckTracker_t** tracker) {
    if (!tracker || !*tracker) return;
    krs_array_destroy_items(&(*tracker)->pending, s_entry_destroy);
    free(*tracker);
    *tracker = NULL;
}

void krs_ack_tracker_expect(AckTracker_t* tracker, uint64_t packet_id,
                            const uint8_t* frame_data, uint16_t frame_size) {
    if (!tracker) return;

    AckEntry_t* entry = malloc(sizeof(AckEntry_t));
    if (!entry) return;

    entry->frame_data = NULL;
    if (frame_data && frame_size > 0) {
        entry->frame_data = malloc(frame_size);
        if (!entry->frame_data) {
            free(entry);
            return;
        }
        memcpy(entry->frame_data, frame_data, frame_size);
    }

    entry->packet_id = packet_id;
    entry->frame_size = frame_size;
    entry->timestamp_ms = GetTickCount64();
    entry->retry_count = 0;

    if (krs_array_push(tracker->pending, entry).base.error_code != KRS_SUCCESS) {
        s_entry_destroy(entry);
    }
}

void krs_ack_tracker_receive(AckTracker_t* tracker, uint64_t acked_packet_id) {
    if (!tracker) return;

    uint32_t i = krs_array_length(tracker->pending);
    while (i > 0) {
        i--;
        AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        if (entry && entry->packet_id == acked_packet_id) {
            s_entry_destroy(entry);
            krs_array_remove(tracker->pending, i);
        }
    }
}

double krs_ack_tracker_receive_rtt(AckTracker_t* tracker, uint64_t acked_packet_id) {
    if (!tracker) return -1.0;

    double rtt_ms = -1.0;
    uint64_t now = GetTickCount64();
    uint32_t i = krs_array_length(tracker->pending);

    while (i > 0) {
        i--;
        AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        if (entry && entry->packet_id == acked_packet_id) {
            if (rtt_ms < 0.0) {
                rtt_ms = (double)(now - entry->timestamp_ms);
            }
            s_entry_destroy(entry);
            krs_array_remove(tracker->pending, i);
        }
    }

    return rtt_ms;
}

uint32_t krs_ack_tracker_check_timeouts(AckTracker_t* tracker,
                                        uint64_t* retry_ids_out, uint32_t out_capacity) {
    if (!tracker || !retry_ids_out) return 0;

    uint64_t now = GetTickCount64();
    uint32_t retry_count = 0;
    uint32_t i = krs_array_length(tracker->pending);

    while (i > 0) {
        i--;
        AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        if (!entry) continue;
        if (now - entry->timestamp_ms < tracker->timeout_ms) continue;

        if (entry->retry_count >= tracker->max_retries) {
            s_entry_destroy(entry);
            krs_array_remove(tracker->pending, i);
            continue;
        }

        if (retry_count < out_capacity) {
            retry_ids_out[retry_count] = entry->packet_id;
        }
        retry_count++;
        entry->retry_count++;
        entry->timestamp_ms = now;
    }

    return retry_count;
}

const uint8_t* krs_ack_tracker_get_retry_frame(const AckTracker_t* tracker,
                                                uint64_t packet_id,
                                                uint16_t* frame_size_out) {
    if (frame_size_out) *frame_size_out = 0;
    if (!tracker || !tracker->pending) return NULL;

    uint32_t len = krs_array_length(tracker->pending);
    for (uint32_t i = 0; i < len; i++) {
        AckEntry_t* entry = KRS_ARRAY_GET(tracker->pending, i, AckEntry_t);
        if (!entry || entry->packet_id != packet_id) continue;
        if (!entry->frame_data || entry->frame_size == 0) continue;
        if (frame_size_out) *frame_size_out = entry->frame_size;
        return entry->frame_data;
    }
    return NULL;
}

void krs_ack_tracker_set_timeout(AckTracker_t* tracker, uint32_t timeout_ms) {
    if (!tracker) return;
    if (timeout_ms < 50) timeout_ms = 50;
    tracker->timeout_ms = timeout_ms;
}
