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

add_executable(kronos_preview_server server.c)
add_executable(kronos_preview_client client.c)
add_executable(kronos_test test_app.c)

target_link_libraries(kronos_test PRIVATE kronos ws2_32)

if (WIN32)
    target_link_libraries(kronos_preview_server PRIVATE kronos ws2_32)
    target_link_libraries(kronos_preview_client PRIVATE kronos ws2_32)
endif ()


target_include_directories(kronos PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(kronos PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

target_compile_definitions(kronos PUBLIC
        KRONOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        KRONOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        KRONOS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
