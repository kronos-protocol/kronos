cmake_minimum_required(VERSION 3.31)
project(kronos VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
endif()

add_library(kronos STATIC
        src/frame/frame_lifecycle.c
        src/frame/frame_builder.c
        src/frame/frame_version.c
        src/math/bitmask.c
        src/frame/frame_utility.c
        src/frame/frame_content.c
        src/math/uint_conversion.c
        src/connection/server/server_port_manager_lifecycle.c
        src/connection/server/server_port_manager_validation.c
        src/connection/server/socket_handler_lifecycle.c
        src/connection/network/address_lifecycle.c
        src/connection/network/udp_socket_ref_lifecycle.c
        src/connection/network/wsa_lifecycle.c
        src/connection/network/address_operations.c
        src/connection/network/net_send.c
        src/lib/port_table/port_table_lifecycle.c
        src/lib/port_table/port_table_manipulation.c
        src/log/logging.c
        src/lib/port_table/link_hash_port_index.c
        src/lib/port_table/port_table_interaction.c
        src/error/error_messages.c
        src/lib/array/array_lifecycle.c
        src/lib/array/array_operations.c
        src/ack/ack_tracker.c
        src/packet/packet_counter.c
        src/congestion/congestion_controller.c
        src/frame/fragment.c
        src/connection/server/server_callbacks.c
        src/connection/server/server_broadcast.c
        src/connection/server/server_connection_handler.c
        src/connection/server/message_queue.c
        src/connection/server/iocp_lifecycle.c
        src/connection/server/iocp_io_thread.c
        src/connection/server/message_handler.c
        src/connection/server/retransmit_thread.c
        src/connection/server/message_pool.c
        src/connection/server/connection_map.c
        src/connection/client/client_connection.c
)

target_link_libraries(kronos PRIVATE ws2_32)

target_include_directories(kronos PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(kronos PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

target_compile_definitions(kronos PUBLIC
        KRONOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        KRONOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        KRONOS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

enable_testing()

add_library(unity STATIC unity/src/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity/src)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

add_library(cmock STATIC cmock/src/cmock.c)
target_include_directories(cmock PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/cmock/src
        ${CMAKE_CURRENT_SOURCE_DIR}/unity
)
target_link_libraries(cmock PUBLIC unity)

set(CMOCK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmock)
set(MOCK_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/mocks)
file(MAKE_DIRECTORY ${MOCK_OUTPUT_DIR})
set(GENERATED_MOCKS "")

set(MOCK_FORCE_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/mock_force_include.h)
set(MOCK_CONTENT "// Auto-generated - force include all internal headers\n")

file(GLOB_RECURSE INTERNAL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/internal/*.h")
foreach(h ${INTERNAL_HEADERS})
    file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}/internal" "${h}")
    string(APPEND MOCK_CONTENT "#include \"${rel_path}\"\n")
endforeach()

if(EXISTS "${MOCK_FORCE_INCLUDE}")
    file(READ "${MOCK_FORCE_INCLUDE}" OLD_CONTENT)
else()
    set(OLD_CONTENT "")
endif()

if(NOT "${OLD_CONTENT}" STREQUAL "${MOCK_CONTENT}")
    file(WRITE "${MOCK_FORCE_INCLUDE}" "${MOCK_CONTENT}")
endif()

set(CMOCK_CONFIG ${CMAKE_CURRENT_BINARY_DIR}/cmock_config.yml)
file(WRITE ${CMOCK_CONFIG} "")
file(APPEND ${CMOCK_CONFIG} ":cmock:\n")
file(APPEND ${CMOCK_CONFIG} "  :mock_path: '${MOCK_OUTPUT_DIR}'\n")
file(APPEND ${CMOCK_CONFIG} "  :mock_prefix: 'mock_'\n")
file(APPEND ${CMOCK_CONFIG} "  :plugins:\n")
file(APPEND ${CMOCK_CONFIG} "  - :ignore\n")
file(APPEND ${CMOCK_CONFIG} "  - :callback\n")
file(APPEND ${CMOCK_CONFIG} "  - :return_thru_ptr\n")
file(APPEND ${CMOCK_CONFIG} "  :exclude_functions:\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_port_manager_create\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_port_manager_destroy\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_port_manager_port_add\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_port_manager_port_add_with_address\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_udp_socket_handler_create\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_set_port_callback\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_set_channel_callback\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_broadcast\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_broadcast_except\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_start\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_stop\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_send\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_client_set_callback\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_client_start_receive\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_set_connect_callback\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_set_disconnect_callback\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_ack_tracker_get_retry_frame\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_ack_tracker_receive_rtt\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_create\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_destroy\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_can_send\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_on_send\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_on_ack\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_on_loss\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_get_cwnd\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_get_in_flight\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_get_rto\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_congestion_get_srtt\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_reassembler_sweep_stale\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_set_thread_counts\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_ack_tracker_set_timeout\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_client_send_blocking\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_send_blocking\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_get_stats\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_server_set_channel_range_callback\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_log_set_level\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_log_get_level\n")
file(APPEND ${CMOCK_CONFIG} "    - krs_message_pool_get_fallback_count\n")
file(APPEND ${CMOCK_CONFIG} "  :treat_as:\n")
file(APPEND ${CMOCK_CONFIG} "    uint8_t: HEX8\n")
file(APPEND ${CMOCK_CONFIG} "    uint16_t: HEX16\n")
file(APPEND ${CMOCK_CONFIG} "    uint32_t: HEX32\n")
file(APPEND ${CMOCK_CONFIG} "  :strippables:\n")
file(APPEND ${CMOCK_CONFIG} "  - '(?:EXTERN_C_START|EXTERN_C_END)'\n")

function(add_mock_for_header HEADER_FILE)
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME_WE)
    set(MOCK_OUTPUT ${MOCK_OUTPUT_DIR}/mock_${HEADER_NAME}.c)

    get_filename_component(HEADER_ABS "${HEADER_FILE}" ABSOLUTE)
    get_filename_component(CONFIG_ABS "${CMOCK_CONFIG}" ABSOLUTE)

    if(WIN32)
        set(PATH_SEP ";")
    else()
        set(PATH_SEP ":")
    endif()

    add_custom_command(
            OUTPUT ${MOCK_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E env "C_INCLUDE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/internal${PATH_SEP}${CMAKE_CURRENT_SOURCE_DIR}/include"
                    ruby cmock.rb -o${CONFIG_ABS} ${HEADER_ABS}
            DEPENDS ${HEADER_FILE} ${CMOCK_CONFIG}
            COMMENT "Generating mock for ${HEADER_NAME}"
            WORKING_DIRECTORY ${CMOCK_DIR}/lib
    )

    set(GENERATED_MOCKS ${GENERATED_MOCKS} ${MOCK_OUTPUT} PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
foreach(header ${PUBLIC_HEADERS})
    add_mock_for_header(${header})
endforeach()

file(GLOB_RECURSE PRIVATE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/internal/*.h")
foreach(header ${PRIVATE_HEADERS})
    add_mock_for_header(${header})
endforeach()

add_library(kronos_mocks STATIC ${GENERATED_MOCKS})

target_include_directories(kronos_mocks PUBLIC
        ${MOCK_OUTPUT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

target_link_libraries(kronos_mocks PUBLIC cmock unity)

target_compile_options(kronos_mocks PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI${MOCK_FORCE_INCLUDE}>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-include${MOCK_FORCE_INCLUDE}>
)

add_custom_target(clean_mocks
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MOCK_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MOCK_OUTPUT_DIR}
        COMMENT "Cleaning old CMock mocks"
)

add_custom_target(generate_mocks ALL DEPENDS ${GENERATED_MOCKS})

add_library(malloc_wrapper STATIC test/mocks/malloc_wrapper.c)
target_include_directories(malloc_wrapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)

add_executable(kronos_unit_tests
        test/main_test.c
        test/port_table_test.c
        test/test_array.c
        test/test_frame.c
        test/test_ack.c
        test/test_packet_counter.c
        test/test_fragment.c
        test/test_connection.c
        test/test_message_queue.c
        test/test_address_compare.c
        test/test_message_handler.c
        test/test_wsa_lifecycle.c
        test/test_integration.c
        test/test_congestion.c
)


# kronos_mocks intentionally excluded — causes duplicate symbol errors with direct kronos link
target_link_libraries(kronos_unit_tests PRIVATE kronos unity malloc_wrapper ws2_32)

target_link_options(kronos_unit_tests PRIVATE
        -Wl,--wrap=malloc
        -Wl,--wrap=calloc
        -Wl,--wrap=free
        -Wl,--wrap=realloc
        --coverage
)

target_compile_options(kronos_unit_tests PRIVATE --coverage)

target_include_directories(kronos_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks
        ${MOCK_OUTPUT_DIR}
)

add_test(NAME kronos_unit_tests COMMAND kronos_unit_tests)

# Demo applications
add_executable(demo_server demo/demo_server.c)
target_link_libraries(demo_server PRIVATE kronos ws2_32)
target_include_directories(demo_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

add_executable(demo_client demo/demo_client.c)
target_link_libraries(demo_client PRIVATE kronos ws2_32)
target_include_directories(demo_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

# Benchmark suite
add_executable(bench_server bench/bench_server.c)
target_link_libraries(bench_server PRIVATE kronos ws2_32)
target_include_directories(bench_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

add_executable(bench_client bench/bench_client.c bench/bench_stats.c)
target_link_libraries(bench_client PRIVATE kronos ws2_32)
target_include_directories(bench_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

add_executable(bench_multi bench/bench_multi.c bench/bench_stats.c)
target_link_libraries(bench_multi PRIVATE kronos ws2_32)
target_include_directories(bench_multi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/bench
)

add_executable(kronos_race_test test/test_race_disconnect_send.c)
target_link_libraries(kronos_race_test PRIVATE kronos ws2_32)
target_include_directories(kronos_race_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

add_executable(lossy_proxy tools/lossy_proxy/lossy_proxy.c)
target_link_libraries(lossy_proxy PRIVATE ws2_32)
