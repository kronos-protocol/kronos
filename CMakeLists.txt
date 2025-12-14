cmake_minimum_required(VERSION 3.31)
project(kronos VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
endif()

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
        src/log/logging.c
        src/lib/port_table/link_hash_port_index.c
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
file(APPEND ${CMOCK_CONFIG} "    - krs_server_channel_handler_create_st\n")
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
)


target_link_libraries(kronos_unit_tests PRIVATE kronos kronos_mocks unity malloc_wrapper)

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