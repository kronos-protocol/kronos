#ifndef CONGESTION_INTERNAL_H
#define CONGESTION_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { CC_SLOW_START, CC_CONGESTION_AVOIDANCE } CongestionPhase_e;

#define KRS_CC_INITIAL_CWND       4
#define KRS_CC_INITIAL_SSTHRESH   64
#define KRS_CC_MIN_CWND           1
#define KRS_CC_ALPHA              0.125
#define KRS_CC_BETA               0.25
#define KRS_CC_RTO_MIN_MS         200.0
#define KRS_CC_RTO_MAX_MS         60000.0
#define KRS_CC_INITIAL_RTO_MS     1000.0

struct CongestionController {
    double              cwnd;
    double              ssthresh;
    uint32_t            in_flight;
    double              srtt_ms;
    double              rttvar_ms;
    double              rto_ms;
    bool                has_rtt_sample;
    CongestionPhase_e   phase;
};

#endif // CONGESTION_INTERNAL_H
