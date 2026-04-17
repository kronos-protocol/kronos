#include "kronos_congestion.h"

#include "congestion_internal.h"

#include <stdlib.h>
#include <math.h>

CongestionController_t* krs_congestion_create(void) {
    CongestionController_t* cc = malloc(sizeof(CongestionController_t));
    if (!cc) return NULL;
    cc->cwnd = KRS_CC_INITIAL_CWND;
    cc->ssthresh = KRS_CC_INITIAL_SSTHRESH;
    cc->in_flight = 0;
    cc->srtt_ms = 0.0;
    cc->rttvar_ms = 0.0;
    cc->rto_ms = KRS_CC_INITIAL_RTO_MS;
    cc->has_rtt_sample = false;
    cc->phase = CC_SLOW_START;
    return cc;
}

void krs_congestion_destroy(CongestionController_t** cc) {
    if (!cc || !*cc) return;
    free(*cc);
    *cc = NULL;
}

bool krs_congestion_can_send(const CongestionController_t* cc) {
    if (!cc) return false;
    return cc->in_flight < (uint32_t)cc->cwnd;
}

void krs_congestion_on_send(CongestionController_t* cc) {
    if (!cc) return;
    cc->in_flight++;
}

void krs_congestion_on_ack(CongestionController_t* cc, double rtt_ms) {
    if (!cc) return;
    if (cc->in_flight > 0) cc->in_flight--;

    if (!cc->has_rtt_sample) {
        cc->srtt_ms = rtt_ms;
        cc->rttvar_ms = rtt_ms / 2.0;
        cc->has_rtt_sample = true;
    } else {
        cc->rttvar_ms = (1.0 - KRS_CC_BETA) * cc->rttvar_ms +
                        KRS_CC_BETA * fabs(cc->srtt_ms - rtt_ms);
        cc->srtt_ms = (1.0 - KRS_CC_ALPHA) * cc->srtt_ms +
                      KRS_CC_ALPHA * rtt_ms;
    }

    cc->rto_ms = cc->srtt_ms + 4.0 * cc->rttvar_ms;
    if (cc->rto_ms < KRS_CC_RTO_MIN_MS) cc->rto_ms = KRS_CC_RTO_MIN_MS;
    if (cc->rto_ms > KRS_CC_RTO_MAX_MS) cc->rto_ms = KRS_CC_RTO_MAX_MS;

    if (cc->phase == CC_SLOW_START) {
        cc->cwnd += 1.0;
        if (cc->cwnd >= cc->ssthresh) {
            cc->phase = CC_CONGESTION_AVOIDANCE;
        }
    } else {
        cc->cwnd += 1.0 / cc->cwnd;
    }
}

void krs_congestion_on_loss(CongestionController_t* cc) {
    if (!cc) return;
    cc->ssthresh = cc->cwnd / 2.0;
    if (cc->ssthresh < KRS_CC_MIN_CWND) cc->ssthresh = KRS_CC_MIN_CWND;
    cc->cwnd = KRS_CC_MIN_CWND;
    cc->phase = CC_SLOW_START;
}

uint32_t krs_congestion_get_cwnd(const CongestionController_t* cc) {
    if (!cc) return 0;
    return (uint32_t)cc->cwnd;
}

uint32_t krs_congestion_get_in_flight(const CongestionController_t* cc) {
    if (!cc) return 0;
    return cc->in_flight;
}

double krs_congestion_get_rto(const CongestionController_t* cc) {
    if (!cc) return KRS_CC_INITIAL_RTO_MS;
    return cc->rto_ms;
}

double krs_congestion_get_srtt(const CongestionController_t* cc) {
    if (!cc) return 0.0;
    return cc->srtt_ms;
}
