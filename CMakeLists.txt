cmake_minimum_required(VERSION 3.31)
project(kronos VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_library(kronos STATIC
        src/frame.c
        server.c
)

add_executable(kronos_preview_server server.c)
add_executable(kronos_preview_client client.c)

if(WIN32)
    target_link_libraries(kronos_preview_server ws2_32)
    target_link_libraries(kronos_preview_client ws2_32)
endif()


target_include_directories(kronos PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(kronos PUBLIC
        KRONOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        KRONOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        KRONOS_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
