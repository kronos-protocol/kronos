#include "kronos_congestion.h"
#include "congestion_internal.h"
#include "malloc_wrapper.h"

#include <unity.h>
#include <math.h>

void test_cc_create_destroy(void) {
    CongestionController_t* cc = krs_congestion_create();
    TEST_ASSERT_NOT_NULL(cc);
    TEST_ASSERT_EQUAL_UINT32(KRS_CC_INITIAL_CWND, krs_congestion_get_cwnd(cc));
    TEST_ASSERT_EQUAL_UINT32(0, krs_congestion_get_in_flight(cc));
    TEST_ASSERT_DOUBLE_WITHIN(0.1, KRS_CC_INITIAL_RTO_MS, krs_congestion_get_rto(cc));
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 0.0, krs_congestion_get_srtt(cc));
    krs_congestion_destroy(&cc);
    TEST_ASSERT_NULL(cc);
}

void test_cc_destroy_null(void) {
    krs_congestion_destroy(NULL);
    CongestionController_t* cc = NULL;
    krs_congestion_destroy(&cc);
}

void test_cc_can_send_within_window(void) {
    CongestionController_t* cc = krs_congestion_create();
    TEST_ASSERT_TRUE(krs_congestion_can_send(cc));
    for (int i = 0; i < KRS_CC_INITIAL_CWND; i++) {
        TEST_ASSERT_TRUE(krs_congestion_can_send(cc));
        krs_congestion_on_send(cc);
    }
    TEST_ASSERT_FALSE(krs_congestion_can_send(cc));
    krs_congestion_destroy(&cc);
}

void test_cc_can_send_null(void) {
    TEST_ASSERT_FALSE(krs_congestion_can_send(NULL));
}

void test_cc_slow_start_doubles_per_rtt(void) {
    CongestionController_t* cc = krs_congestion_create();
    uint32_t initial = krs_congestion_get_cwnd(cc);
    for (uint32_t i = 0; i < initial; i++) {
        krs_congestion_on_send(cc);
    }
    for (uint32_t i = 0; i < initial; i++) {
        krs_congestion_on_ack(cc, 50.0);
    }
    TEST_ASSERT_EQUAL_UINT32(initial * 2, krs_congestion_get_cwnd(cc));
    krs_congestion_destroy(&cc);
}

void test_cc_slow_start_to_avoidance_transition(void) {
    CongestionController_t* cc = krs_congestion_create();
    uint32_t acks_needed = KRS_CC_INITIAL_SSTHRESH - KRS_CC_INITIAL_CWND;
    for (uint32_t i = 0; i < acks_needed; i++) {
        krs_congestion_on_send(cc);
        krs_congestion_on_ack(cc, 50.0);
    }
    TEST_ASSERT_TRUE(krs_congestion_get_cwnd(cc) >= KRS_CC_INITIAL_SSTHRESH);
    TEST_ASSERT_EQUAL(CC_CONGESTION_AVOIDANCE, cc->phase);
    krs_congestion_destroy(&cc);
}

void test_cc_avoidance_linear_growth(void) {
    CongestionController_t* cc = krs_congestion_create();
    for (uint32_t i = 0; i < KRS_CC_INITIAL_SSTHRESH; i++) {
        krs_congestion_on_send(cc);
        krs_congestion_on_ack(cc, 50.0);
    }
    TEST_ASSERT_EQUAL(CC_CONGESTION_AVOIDANCE, cc->phase);
    uint32_t before = krs_congestion_get_cwnd(cc);
    uint32_t cwnd_now = before;
    for (uint32_t i = 0; i < cwnd_now; i++) {
        krs_congestion_on_send(cc);
        krs_congestion_on_ack(cc, 50.0);
    }
    TEST_ASSERT_EQUAL_UINT32(before + 1, krs_congestion_get_cwnd(cc));
    krs_congestion_destroy(&cc);
}

void test_cc_loss_halves_window(void) {
    CongestionController_t* cc = krs_congestion_create();
    for (uint32_t i = 0; i < 20; i++) {
        krs_congestion_on_send(cc);
        krs_congestion_on_ack(cc, 50.0);
    }
    uint32_t before = krs_congestion_get_cwnd(cc);
    TEST_ASSERT_TRUE(before > KRS_CC_MIN_CWND);
    krs_congestion_on_loss(cc);
    TEST_ASSERT_EQUAL_UINT32(KRS_CC_MIN_CWND, krs_congestion_get_cwnd(cc));
    TEST_ASSERT_TRUE(cc->ssthresh >= before / 2 - 1 && cc->ssthresh <= before / 2 + 1);
    TEST_ASSERT_EQUAL(CC_SLOW_START, cc->phase);
    krs_congestion_destroy(&cc);
}

void test_cc_multiple_losses_floor(void) {
    CongestionController_t* cc = krs_congestion_create();
    krs_congestion_on_loss(cc);
    krs_congestion_on_loss(cc);
    krs_congestion_on_loss(cc);
    TEST_ASSERT_TRUE(krs_congestion_get_cwnd(cc) >= KRS_CC_MIN_CWND);
    TEST_ASSERT_TRUE(cc->ssthresh >= KRS_CC_MIN_CWND);
    krs_congestion_destroy(&cc);
}

void test_cc_rtt_first_sample(void) {
    CongestionController_t* cc = krs_congestion_create();
    krs_congestion_on_send(cc);
    krs_congestion_on_ack(cc, 100.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 100.0, krs_congestion_get_srtt(cc));
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 50.0, cc->rttvar_ms);
    krs_congestion_destroy(&cc);
}

void test_cc_rtt_converges(void) {
    CongestionController_t* cc = krs_congestion_create();
    for (int i = 0; i < 20; i++) {
        krs_congestion_on_send(cc);
        krs_congestion_on_ack(cc, 40.0);
    }
    TEST_ASSERT_DOUBLE_WITHIN(5.0, 40.0, krs_congestion_get_srtt(cc));
    krs_congestion_destroy(&cc);
}

void test_cc_rto_clamping(void) {
    CongestionController_t* cc = krs_congestion_create();
    krs_congestion_on_send(cc);
    krs_congestion_on_ack(cc, 0.001);
    TEST_ASSERT_TRUE(krs_congestion_get_rto(cc) >= KRS_CC_RTO_MIN_MS);
    krs_congestion_destroy(&cc);
    cc = krs_congestion_create();
    krs_congestion_on_send(cc);
    krs_congestion_on_ack(cc, 100000.0);
    TEST_ASSERT_TRUE(krs_congestion_get_rto(cc) <= KRS_CC_RTO_MAX_MS);
    krs_congestion_destroy(&cc);
}

void test_cc_on_send_increments_in_flight(void) {
    CongestionController_t* cc = krs_congestion_create();
    TEST_ASSERT_EQUAL_UINT32(0, krs_congestion_get_in_flight(cc));
    krs_congestion_on_send(cc);
    TEST_ASSERT_EQUAL_UINT32(1, krs_congestion_get_in_flight(cc));
    krs_congestion_on_send(cc);
    TEST_ASSERT_EQUAL_UINT32(2, krs_congestion_get_in_flight(cc));
    krs_congestion_on_ack(cc, 50.0);
    TEST_ASSERT_EQUAL_UINT32(1, krs_congestion_get_in_flight(cc));
    krs_congestion_destroy(&cc);
}

void test_cc_create_malloc_failure(void) {
    mock_malloc_fail_next();
    CongestionController_t* cc = krs_congestion_create();
    TEST_ASSERT_NULL(cc);
}
