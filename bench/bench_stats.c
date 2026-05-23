#include "bench_common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

struct BenchStats {
    double*  samples;
    uint32_t count;
    uint32_t capacity;
    uint64_t msg_sent;
    uint64_t msg_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    double   duration_s;
};

BenchStats_t* bench_stats_create(uint32_t max_samples) {
    BenchStats_t* s = calloc(1, sizeof(BenchStats_t));
    if (!s) return NULL;
    if (max_samples > 0) {
        s->samples = malloc(max_samples * sizeof(double));
        if (!s->samples) { free(s); return NULL; }
    }
    s->capacity = max_samples;
    return s;
}

void bench_stats_destroy(BenchStats_t** stats) {
    if (!stats || !*stats) return;
    free((*stats)->samples);
    free(*stats);
    *stats = NULL;
}

void bench_stats_record_latency(BenchStats_t* stats, double latency_us) {
    if (!stats || !stats->samples || stats->count >= stats->capacity) return;
    stats->samples[stats->count++] = latency_us;
}

void bench_stats_set_counts(BenchStats_t* stats, uint64_t sent, uint64_t received,
                            uint64_t bytes_sent, uint64_t bytes_received,
                            double duration_s) {
    if (!stats) return;
    stats->msg_sent = sent;
    stats->msg_received = received;
    stats->bytes_sent = bytes_sent;
    stats->bytes_received = bytes_received;
    stats->duration_s = duration_s;
}

static int s_double_cmp(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

static double s_percentile(const double* sorted, uint32_t count, double pct) {
    if (count == 0) return 0.0;
    double idx = (pct / 100.0) * (count - 1);
    uint32_t lo = (uint32_t)idx;
    uint32_t hi = lo + 1;
    if (hi >= count) return sorted[count - 1];
    double frac = idx - (double)lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

BenchResult_t bench_stats_compute(BenchStats_t* stats) {
    BenchResult_t r = {0};
    if (!stats) return r;

    r.msg_sent = stats->msg_sent;
    r.msg_received = stats->msg_received;
    r.msg_lost = (stats->msg_sent > stats->msg_received)
                     ? stats->msg_sent - stats->msg_received
                     : 0;
    r.bytes_sent = stats->bytes_sent;
    r.bytes_received = stats->bytes_received;
    r.duration_s = stats->duration_s;

    if (stats->duration_s > 0.0) {
        r.send_rate_msg_s = (double)stats->msg_sent / stats->duration_s;
        r.recv_rate_msg_s = (double)stats->msg_received / stats->duration_s;
        r.send_bw_mbps = (double)stats->bytes_sent / stats->duration_s / (1024.0 * 1024.0);
        r.recv_bw_mbps = (double)stats->bytes_received / stats->duration_s / (1024.0 * 1024.0);
    }

    if (stats->count == 0) return r;

    qsort(stats->samples, stats->count, sizeof(double), s_double_cmp);

    r.min_us    = stats->samples[0];
    r.max_us    = stats->samples[stats->count - 1];
    r.median_us = s_percentile(stats->samples, stats->count, 50.0);
    r.p95_us    = s_percentile(stats->samples, stats->count, 95.0);
    r.p99_us    = s_percentile(stats->samples, stats->count, 99.0);

    double sum = 0.0;
    for (uint32_t i = 0; i < stats->count; i++) sum += stats->samples[i];
    r.avg_us = sum / stats->count;

    double var_sum = 0.0;
    for (uint32_t i = 0; i < stats->count; i++) {
        double d = stats->samples[i] - r.avg_us;
        var_sum += d * d;
    }
    r.stddev_us = sqrt(var_sum / stats->count);

    if (stats->count > 1) {
        double jitter_sum = 0.0;
        for (uint32_t i = 1; i < stats->count; i++) {
            jitter_sum += fabs(stats->samples[i] - stats->samples[i - 1]);
        }
        r.jitter_us = jitter_sum / (stats->count - 1);
    }

    return r;
}

void bench_stats_print(const BenchResult_t* r, const char* test_name, uint32_t msg_size) {
    if (!r || !test_name) return;
    printf("\n");
    printf("========================================\n");
    printf("  Kronos Benchmark: %s\n", test_name);
    printf("========================================\n");
    if (msg_size > 0)
        printf("  Message size:   %u bytes\n", msg_size);
    printf("  Duration:       %.3f s\n", r->duration_s);
    printf("  Sent:           %llu msg\n", (unsigned long long)r->msg_sent);
    printf("  Received:       %llu msg\n", (unsigned long long)r->msg_received);
    if (r->msg_lost > 0)
        printf("  Lost:           %llu msg (%.2f%%)\n",
               (unsigned long long)r->msg_lost,
               r->msg_sent > 0 ? 100.0 * (double)r->msg_lost / (double)r->msg_sent : 0.0);
    printf("  Send rate:      %.1f msg/s\n", r->send_rate_msg_s);
    if (r->recv_rate_msg_s > 0.0)
        printf("  Recv rate:      %.1f msg/s\n", r->recv_rate_msg_s);
    printf("  Send bandwidth: %.2f MB/s\n", r->send_bw_mbps);

    if (r->avg_us > 0.0) {
        printf("  ---- Latency (microseconds) ----\n");
        printf("  Min:    %.1f\n", r->min_us);
        printf("  Avg:    %.1f\n", r->avg_us);
        printf("  Median: %.1f\n", r->median_us);
        printf("  P95:    %.1f\n", r->p95_us);
        printf("  P99:    %.1f\n", r->p99_us);
        printf("  Max:    %.1f\n", r->max_us);
        printf("  StdDev: %.1f\n", r->stddev_us);
        printf("  Jitter: %.1f\n", r->jitter_us);
    }
    printf("========================================\n\n");
}

BenchResult_t bench_stats_merge_results(const BenchResult_t* results, uint32_t count) {
    BenchResult_t merged = {0};
    if (!results || count == 0) return merged;

    merged.min_us = results[0].min_us;
    merged.max_us = results[0].max_us;

    double latency_sum = 0.0;
    uint32_t latency_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        const BenchResult_t* r = &results[i];
        merged.msg_sent += r->msg_sent;
        merged.msg_received += r->msg_received;
        merged.msg_lost += r->msg_lost;
        merged.bytes_sent += r->bytes_sent;
        merged.bytes_received += r->bytes_received;
        if (r->duration_s > merged.duration_s) merged.duration_s = r->duration_s;
        if (r->min_us < merged.min_us) merged.min_us = r->min_us;
        if (r->max_us > merged.max_us) merged.max_us = r->max_us;
        if (r->avg_us > 0.0) {
            latency_sum += r->avg_us * r->msg_received;
            latency_count += (uint32_t)r->msg_received;
        }
        merged.send_rate_msg_s += r->send_rate_msg_s;
        merged.recv_rate_msg_s += r->recv_rate_msg_s;
        merged.send_bw_mbps += r->send_bw_mbps;
        merged.recv_bw_mbps += r->recv_bw_mbps;
    }

    if (latency_count > 0) merged.avg_us = latency_sum / latency_count;

    double median_sum = 0.0, p95_sum = 0.0, p99_sum = 0.0, stddev_sum = 0.0, jitter_sum = 0.0;
    uint32_t pct_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (results[i].avg_us > 0.0) {
            median_sum += results[i].median_us;
            p95_sum += results[i].p95_us;
            p99_sum += results[i].p99_us;
            stddev_sum += results[i].stddev_us;
            jitter_sum += results[i].jitter_us;
            pct_count++;
        }
    }
    if (pct_count > 0) {
        merged.median_us = median_sum / pct_count;
        merged.p95_us = p95_sum / pct_count;
        merged.p99_us = p99_sum / pct_count;
        merged.stddev_us = stddev_sum / pct_count;
        merged.jitter_us = jitter_sum / pct_count;
    }

    return merged;
}

void bench_stats_export_csv(const BenchStats_t* stats, const char* filename) {
    if (!stats || !filename || stats->count == 0) return;
    FILE* f = fopen(filename, "w");
    if (!f) { printf("Failed to open %s for writing\n", filename); return; }
    fprintf(f, "sample_index,latency_us\n");
    for (uint32_t i = 0; i < stats->count; i++) {
        fprintf(f, "%u,%.1f\n", i, stats->samples[i]);
    }
    fclose(f);
    printf("Exported %u latency samples to %s\n", stats->count, filename);
}
