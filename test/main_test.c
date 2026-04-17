#include "malloc_wrapper.h"
#include <unity.h>

void setUp() {
    mock_malloc_reset();
}

void tearDown() {
    mock_malloc_reset();
}

void test_port_table_internal_prime_sizes_exist(void);
void test_port_table_creation(void);
void test_port_table_creation_s(void);
void test_port_table_insert(void);
void test_port_table_destroy(void);

void test_array_create_destroy(void);
void test_array_create_default_capacity(void);
void test_array_create_s_success(void);
void test_array_create_s_malloc_failure(void);
void test_array_push_get(void);
void test_array_get_out_of_bounds(void);
void test_array_pop(void);
void test_array_auto_grow(void);
void test_array_remove_shifts_left(void);
void test_array_remove_out_of_bounds(void);
void test_array_set(void);
void test_array_is_empty(void);
void test_array_clear(void);
void test_array_destroy_items(void);
void test_array_push_null_array(void);
void test_array_macros(void);

void test_frame_create_valid(void);
void test_frame_create_invalid_magic(void);
void test_frame_create_too_short(void);
void test_frame_create_null_buffer(void);
void test_frame_create_heap_valid(void);
void test_frame_create_heap_invalid(void);
void test_frame_builder_roundtrip(void);
void test_frame_builder_serialize_too_small(void);
void test_version_encode_decode_roundtrip(void);
void test_version_encode_bit_layout(void);
void test_frame_metadata_flag_count(void);

void test_ack_tracker_create_destroy(void);
void test_ack_tracker_destroy_null(void);
void test_ack_tracker_expect_and_receive(void);
void test_ack_tracker_receive_unknown_id(void);
void test_ack_tracker_timeout_retry(void);
void test_ack_tracker_max_retries_drop(void);
void test_ack_tracker_multiple_concurrent(void);
void test_ack_tracker_no_timeout_when_fresh(void);
void test_ack_tracker_create_malloc_failure(void);
void test_ack_tracker_expect_malloc_failure(void);
void test_ack_tracker_expect_null_tracker(void);
void test_ack_tracker_receive_null_tracker(void);
void test_ack_tracker_check_timeouts_null_params(void);
void test_ack_tracker_frame_data_copied(void);
void test_ack_tracker_check_timeouts_capacity_limit(void);
void test_ack_receive_removes_all_entries_with_same_id(void);

void test_packet_counter_create_destroy(void);
void test_packet_counter_starts_at_zero(void);
void test_packet_counter_next_increments(void);
void test_packet_counter_per_channel_independence(void);
void test_packet_counter_reset(void);
void test_packet_counter_reset_isolated(void);
void test_packet_counter_null_safety(void);
void test_packet_counter_create_malloc_failure(void);
void test_packet_counter_boundary_channels(void);

void test_fragment_split_no_frag_when_fits(void);
void test_fragment_split_correct_count(void);
void test_fragment_split_fragment_info_flag_set(void);
void test_fragment_reassemble_in_order(void);
void test_fragment_reassemble_out_of_order(void);
void test_fragment_reassemble_single_frame(void);
void test_fragment_split_invalid_mtu(void);
void test_fragment_split_wire_format(void);
void test_fragment_split_malloc_failure(void);
void test_reassembler_create_malloc_failure(void);
void test_reassembler_destroy_null(void);
void test_reassembler_feed_null_params(void);
void test_reassembler_feed_single_no_fraginfo(void);
void test_reassembler_feed_two_fragments_direct(void);
void test_reassembler_feed_out_of_order_direct(void);
void test_reassembler_feed_duplicate_fragment(void);

void test_spm_create_destroy(void);
void test_spm_create_malloc_failure(void);
void test_set_port_callback_null_spm(void);
void test_set_channel_callback_reserved_channel(void);
void test_set_channel_callback_null_spm(void);
void test_set_channel_callback_no_descriptor(void);
void test_broadcast_null_params(void);
void test_broadcast_except_null_params(void);
void test_client_disconnect_null(void);
void test_client_send_null_conn(void);
void test_client_send_null_data(void);
void test_handle_connection_frame_null_params(void);
void test_handle_heartbeat_frame_null_params(void);

void test_handle_connection_frame_creates_connection(void);
void test_handle_heartbeat_updates_timestamp(void);
void test_handle_heartbeat_unknown_addr_no_update(void);
void test_set_channel_callback_and_verify(void);
void test_set_port_callback_and_verify(void);

void test_mq_create_destroy(void);
void test_mq_create_zero_capacity(void);
void test_mq_push_pop_single(void);
void test_mq_push_pop_fifo_order(void);
void test_mq_pop_timeout_empty(void);
void test_mq_auto_grow(void);
void test_mq_max_capacity_drops_oldest(void);
void test_mq_stop_wakes_consumer(void);
void test_mq_destroy_frees_remaining(void);
void test_mq_push_null_params(void);
void test_mq_pop_null_queue(void);
void test_mq_msg_destroy(void);

void test_addr_eq_same_endpoint(void);
void test_addr_eq_different_port(void);
void test_addr_eq_different_address(void);
void test_addr_eq_null_params(void);
void test_addr_eq_zeroed_padding(void);
void test_addr_eq_ipv4_mapped(void);

void test_handler_routes_connection_frame_to_channel_0(void);
void test_handler_routes_heartbeat_frame_to_channel_1(void);
void test_handler_callback_lookup_channel_override(void);
void test_handler_callback_lookup_port_fallback(void);
void test_handler_body_null_check(void);
void test_handler_reserved_channel_no_callback(void);

void test_wsa_init_cleanup_single(void);
void test_wsa_init_double_cleanup_double(void);
void test_wsa_cleanup_without_init(void);

void test_integration_server_client_roundtrip(void);
void test_integration_server_sends_to_client(void);
void test_integration_connect_disconnect_lifecycle(void);

void test_cc_create_destroy(void);
void test_cc_destroy_null(void);
void test_cc_can_send_within_window(void);
void test_cc_can_send_null(void);
void test_cc_slow_start_doubles_per_rtt(void);
void test_cc_slow_start_to_avoidance_transition(void);
void test_cc_avoidance_linear_growth(void);
void test_cc_loss_halves_window(void);
void test_cc_multiple_losses_floor(void);
void test_cc_rtt_first_sample(void);
void test_cc_rtt_converges(void);
void test_cc_rto_clamping(void);
void test_cc_on_send_increments_in_flight(void);
void test_cc_create_malloc_failure(void);


int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_port_table_internal_prime_sizes_exist);
    RUN_TEST(test_port_table_creation);
    RUN_TEST(test_port_table_creation_s);
    RUN_TEST(test_port_table_insert);
    RUN_TEST(test_port_table_destroy);

    RUN_TEST(test_array_create_destroy);
    RUN_TEST(test_array_create_default_capacity);
    RUN_TEST(test_array_create_s_success);
    RUN_TEST(test_array_create_s_malloc_failure);
    RUN_TEST(test_array_push_get);
    RUN_TEST(test_array_get_out_of_bounds);
    RUN_TEST(test_array_pop);
    RUN_TEST(test_array_auto_grow);
    RUN_TEST(test_array_remove_shifts_left);
    RUN_TEST(test_array_remove_out_of_bounds);
    RUN_TEST(test_array_set);
    RUN_TEST(test_array_is_empty);
    RUN_TEST(test_array_clear);
    RUN_TEST(test_array_destroy_items);
    RUN_TEST(test_array_push_null_array);
    RUN_TEST(test_array_macros);

    RUN_TEST(test_frame_create_valid);
    RUN_TEST(test_frame_create_invalid_magic);
    RUN_TEST(test_frame_create_too_short);
    RUN_TEST(test_frame_create_null_buffer);
    RUN_TEST(test_frame_create_heap_valid);
    RUN_TEST(test_frame_create_heap_invalid);
    RUN_TEST(test_frame_builder_roundtrip);
    RUN_TEST(test_frame_builder_serialize_too_small);
    RUN_TEST(test_version_encode_decode_roundtrip);
    RUN_TEST(test_version_encode_bit_layout);
    RUN_TEST(test_frame_metadata_flag_count);

    RUN_TEST(test_ack_tracker_create_destroy);
    RUN_TEST(test_ack_tracker_destroy_null);
    RUN_TEST(test_ack_tracker_expect_and_receive);
    RUN_TEST(test_ack_tracker_receive_unknown_id);
    RUN_TEST(test_ack_tracker_timeout_retry);
    RUN_TEST(test_ack_tracker_max_retries_drop);
    RUN_TEST(test_ack_tracker_multiple_concurrent);
    RUN_TEST(test_ack_tracker_no_timeout_when_fresh);
    RUN_TEST(test_ack_tracker_create_malloc_failure);
    RUN_TEST(test_ack_tracker_expect_malloc_failure);
    RUN_TEST(test_ack_tracker_expect_null_tracker);
    RUN_TEST(test_ack_tracker_receive_null_tracker);
    RUN_TEST(test_ack_tracker_check_timeouts_null_params);
    RUN_TEST(test_ack_tracker_frame_data_copied);
    RUN_TEST(test_ack_tracker_check_timeouts_capacity_limit);
    RUN_TEST(test_ack_receive_removes_all_entries_with_same_id);

    RUN_TEST(test_packet_counter_create_destroy);
    RUN_TEST(test_packet_counter_starts_at_zero);
    RUN_TEST(test_packet_counter_next_increments);
    RUN_TEST(test_packet_counter_per_channel_independence);
    RUN_TEST(test_packet_counter_reset);
    RUN_TEST(test_packet_counter_reset_isolated);
    RUN_TEST(test_packet_counter_null_safety);
    RUN_TEST(test_packet_counter_create_malloc_failure);
    RUN_TEST(test_packet_counter_boundary_channels);

    RUN_TEST(test_fragment_split_no_frag_when_fits);
    RUN_TEST(test_fragment_split_correct_count);
    RUN_TEST(test_fragment_split_fragment_info_flag_set);
    RUN_TEST(test_fragment_reassemble_in_order);
    RUN_TEST(test_fragment_reassemble_out_of_order);
    RUN_TEST(test_fragment_reassemble_single_frame);
    RUN_TEST(test_fragment_split_invalid_mtu);
    RUN_TEST(test_fragment_split_wire_format);
    RUN_TEST(test_fragment_split_malloc_failure);
    RUN_TEST(test_reassembler_create_malloc_failure);
    RUN_TEST(test_reassembler_destroy_null);
    RUN_TEST(test_reassembler_feed_null_params);
    RUN_TEST(test_reassembler_feed_single_no_fraginfo);
    RUN_TEST(test_reassembler_feed_two_fragments_direct);
    RUN_TEST(test_reassembler_feed_out_of_order_direct);
    RUN_TEST(test_reassembler_feed_duplicate_fragment);

    RUN_TEST(test_spm_create_destroy);
    RUN_TEST(test_spm_create_malloc_failure);
    RUN_TEST(test_set_port_callback_null_spm);
    RUN_TEST(test_set_channel_callback_reserved_channel);
    RUN_TEST(test_set_channel_callback_null_spm);
    RUN_TEST(test_set_channel_callback_no_descriptor);
    RUN_TEST(test_broadcast_null_params);
    RUN_TEST(test_broadcast_except_null_params);
    RUN_TEST(test_client_disconnect_null);
    RUN_TEST(test_client_send_null_conn);
    RUN_TEST(test_client_send_null_data);
    RUN_TEST(test_handle_connection_frame_null_params);
    RUN_TEST(test_handle_heartbeat_frame_null_params);

    RUN_TEST(test_handle_connection_frame_creates_connection);
    RUN_TEST(test_handle_heartbeat_updates_timestamp);
    RUN_TEST(test_handle_heartbeat_unknown_addr_no_update);
    RUN_TEST(test_set_channel_callback_and_verify);
    RUN_TEST(test_set_port_callback_and_verify);

    RUN_TEST(test_mq_create_destroy);
    RUN_TEST(test_mq_create_zero_capacity);
    RUN_TEST(test_mq_push_pop_single);
    RUN_TEST(test_mq_push_pop_fifo_order);
    RUN_TEST(test_mq_pop_timeout_empty);
    RUN_TEST(test_mq_auto_grow);
    RUN_TEST(test_mq_max_capacity_drops_oldest);
    RUN_TEST(test_mq_stop_wakes_consumer);
    RUN_TEST(test_mq_destroy_frees_remaining);
    RUN_TEST(test_mq_push_null_params);
    RUN_TEST(test_mq_pop_null_queue);
    RUN_TEST(test_mq_msg_destroy);

    RUN_TEST(test_addr_eq_same_endpoint);
    RUN_TEST(test_addr_eq_different_port);
    RUN_TEST(test_addr_eq_different_address);
    RUN_TEST(test_addr_eq_null_params);
    RUN_TEST(test_addr_eq_zeroed_padding);
    RUN_TEST(test_addr_eq_ipv4_mapped);

    RUN_TEST(test_handler_routes_connection_frame_to_channel_0);
    RUN_TEST(test_handler_routes_heartbeat_frame_to_channel_1);
    RUN_TEST(test_handler_callback_lookup_channel_override);
    RUN_TEST(test_handler_callback_lookup_port_fallback);
    RUN_TEST(test_handler_body_null_check);
    RUN_TEST(test_handler_reserved_channel_no_callback);

    RUN_TEST(test_wsa_init_cleanup_single);
    RUN_TEST(test_wsa_init_double_cleanup_double);
    RUN_TEST(test_wsa_cleanup_without_init);

    RUN_TEST(test_cc_create_destroy);
    RUN_TEST(test_cc_destroy_null);
    RUN_TEST(test_cc_can_send_within_window);
    RUN_TEST(test_cc_can_send_null);
    RUN_TEST(test_cc_slow_start_doubles_per_rtt);
    RUN_TEST(test_cc_slow_start_to_avoidance_transition);
    RUN_TEST(test_cc_avoidance_linear_growth);
    RUN_TEST(test_cc_loss_halves_window);
    RUN_TEST(test_cc_multiple_losses_floor);
    RUN_TEST(test_cc_rtt_first_sample);
    RUN_TEST(test_cc_rtt_converges);
    RUN_TEST(test_cc_rto_clamping);
    RUN_TEST(test_cc_on_send_increments_in_flight);
    RUN_TEST(test_cc_create_malloc_failure);

    RUN_TEST(test_integration_server_client_roundtrip);
    RUN_TEST(test_integration_server_sends_to_client);
    RUN_TEST(test_integration_connect_disconnect_lifecycle);

    return UNITY_END();
}
