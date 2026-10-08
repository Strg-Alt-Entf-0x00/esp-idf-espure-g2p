cmake_minimum_required(VERSION 3.10)
project(espure_host_test)

set(CMAKE_C_STANDARD 99)
if(MSVC)
    add_compile_options(/bigobj)
endif()

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Host stubs for ESP-IDF specific things
add_definitions(-DESPURE_HOST_TEST -DCONFIG_ESPURE_LANG_DE -DCONFIG_ESPURE_LANG_DE_DE)

file(GLOB SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/dict/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs/*.c"
)

add_library(espure STATIC ${SOURCES})
add_executable(run_test main.c)
target_link_libraries(run_test espure)