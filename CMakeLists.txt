cmake_minimum_required(VERSION 3.31)
project(kronos VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_library(kronos STATIC
        src/frame/frame_lifecycle.c
        src/math/bitmask.c
        src/frame/frame_utility.c
        src/frame/frame_content.c
        src/math/uint_conversion.c
        src/connection/server/server_connection_lifecycle.c
        src/connection/server/server_port_manager_validation.c
        src/connection/network/address_lifecycle.c
        src/lib/port_table/port_table_lifecycle.c
        src/lib/port_table/port_table_manipulation.c
)

add_executable(port_table_test port_table_test.c)
target_link_libraries(port_table_test PRIVATE kronos)

if (WIN32)
    # target_link_libraries(kronos_preview_server PRIVATE kronos ws2_32)
    # target_link_libraries(kronos_preview_client PRIVATE kronos ws2_32)
endif ()


target_include_directories(kronos PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(kronos PUBLIC # change this later to private once we compile the library seperatly
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

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cmock_config.yml
        ":cmock:
  :mock_path: '${MOCK_OUTPUT_DIR}'
  :mock_prefix: 'mock_'
  :includes:
    - unity.h
    - server_internal.h
  :plugins:
    - :ignore
    - :callback
    - :return_thru_ptr
  :treat_as:
    uint8_t: HEX8
    uint16_t: HEX16
    uint32_t: HEX32
  :strippables:
    - '(?:EXTERN_C_START|EXTERN_C_END)'
")

function(add_mock_for_header HEADER_FILE)
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME_WE)
    set(MOCK_OUTPUT ${MOCK_OUTPUT_DIR}/mock_${HEADER_NAME}.c)

    # Get absolute paths (required when changing working directory)
    get_filename_component(HEADER_ABS "${HEADER_FILE}" ABSOLUTE)
    get_filename_component(CONFIG_ABS "${CMAKE_CURRENT_BINARY_DIR}/cmock_config.yml" ABSOLUTE)

    add_custom_command(
            OUTPUT ${MOCK_OUTPUT}
            COMMAND ruby cmock.rb -o${CONFIG_ABS} ${HEADER_ABS}
            DEPENDS ${HEADER_FILE}
            COMMENT "Generating mock for ${HEADER_NAME}"
            WORKING_DIRECTORY ${CMOCK_DIR}/lib
    )

    set(GENERATED_MOCKS ${GENERATED_MOCKS} ${MOCK_OUTPUT} PARENT_SCOPE)
endfunction()

add_mock_for_header(${CMAKE_CURRENT_SOURCE_DIR}/include/kronos_server.h)

add_library(kronos_mocks STATIC ${GENERATED_MOCKS})
target_include_directories(kronos_mocks PUBLIC
        ${MOCK_OUTPUT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
)
target_link_libraries(kronos_mocks PUBLIC cmock unity)

add_library(malloc_wrapper STATIC test/mocks/malloc_wrapper.c)
target_include_directories(malloc_wrapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)

add_executable(kronos_unit_tests
        test/main_test.c
        test/port_table_test.c
)

if(TARGET kronos_mocks)
    target_link_libraries(kronos_unit_tests PRIVATE kronos_mocks)
endif()

target_link_libraries(kronos_unit_tests PRIVATE kronos unity malloc_wrapper)

target_link_options(kronos_unit_tests PRIVATE
        -Wl,--wrap=malloc
        -Wl,--wrap=calloc
        -Wl,--wrap=free
        -Wl,--wrap=realloc
)

target_include_directories(kronos_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks
        ${MOCK_OUTPUT_DIR}
)

add_test(NAME kronos_unit_tests COMMAND kronos_unit_tests)