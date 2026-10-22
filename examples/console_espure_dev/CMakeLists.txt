cmake_minimum_required(VERSION 3.16)

get_filename_component(ESPURE_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(UTF8_CONSOLE_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../common/uart_utf8_console" ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${ESPURE_COMPONENT_DIR}"
    "${UTF8_CONSOLE_COMPONENT_DIR}"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(console_espure_dev)
