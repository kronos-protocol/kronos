#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

/* ---- High-resolution timer (QueryPerformanceCounter) ---- */

static LARGE_INTEGER s_bench_qpc_freq = {0};

static inline void bench_timer_init(void) {
    QueryPerformanceFrequency(&s_bench_qpc_freq);
}

static inline uint64_t bench_time_us(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart / (double)s_bench_qpc_freq.QuadPart * 1000000.0);
}

/* ---- Benchmark message wire format ----
 * [0-7]   uint64 sequence number (big-endian)
 * [8-15]  uint64 client timestamp in microseconds (big-endian)
 * [16..]  zero padding to desired message size
 */

#define BENCH_MSG_HEADER_SIZE 16
#define BENCH_CHANNEL 11

static inline void bench_msg_encode(uint8_t* buf, uint64_t seq, uint64_t timestamp_us) {
    for (int i = 0; i < 8; i++) buf[i]     = (uint8_t)((seq >> ((7 - i) * 8)) & 0xFF);
    for (int i = 0; i < 8; i++) buf[8 + i]  = (uint8_t)((timestamp_us >> ((7 - i) * 8)) & 0xFF);
}

static inline void bench_msg_decode(const uint8_t* buf, uint64_t* seq, uint64_t* timestamp_us) {
    *seq = 0;
    *timestamp_us = 0;
    for (int i = 0; i < 8; i++) *seq           = (*seq << 8) | buf[i];
    for (int i = 0; i < 8; i++) *timestamp_us   = (*timestamp_us << 8) | buf[8 + i];
}

/* ---- Stats ---- */

typedef struct {
    double   min_us, max_us, avg_us, median_us, p95_us, p99_us, stddev_us;
    double   jitter_us;
    uint64_t msg_sent, msg_received, msg_lost;
    uint64_t bytes_sent, bytes_received;
    double   duration_s;
    double   send_rate_msg_s, recv_rate_msg_s;
    double   send_bw_mbps, recv_bw_mbps;
} BenchResult_t;

typedef struct BenchStats BenchStats_t;

BenchStats_t* bench_stats_create(uint32_t max_samples);
void          bench_stats_destroy(BenchStats_t** stats);
void          bench_stats_record_latency(BenchStats_t* stats, double latency_us);
void          bench_stats_set_counts(BenchStats_t* stats, uint64_t sent, uint64_t received,
                                     uint64_t bytes_sent, uint64_t bytes_received,
                                     double duration_s);
BenchResult_t bench_stats_compute(BenchStats_t* stats);
void          bench_stats_print(const BenchResult_t* result, const char* test_name,
                                uint32_t msg_size);
void          bench_stats_export_csv(const BenchStats_t* stats, const char* filename);
BenchResult_t bench_stats_merge_results(const BenchResult_t* results, uint32_t count);

/* ---- Rate limiter ---- */

typedef struct {
    uint64_t interval_us;
    uint64_t next_send_us;
} RateLimiter_t;

static inline RateLimiter_t bench_rate_limiter_create(uint32_t rate_per_second) {
    RateLimiter_t rl = {0};
    if (rate_per_second > 0) {
        rl.interval_us = 1000000ULL / (uint64_t)rate_per_second;
        rl.next_send_us = bench_time_us();
    }
    return rl;
}

static inline void bench_rate_limiter_wait(RateLimiter_t* rl) {
    if (rl->interval_us == 0) return;
    while (bench_time_us() < rl->next_send_us) {
        Sleep(0);
    }
    rl->next_send_us += rl->interval_us;
}

#endif /* BENCH_COMMON_H */
