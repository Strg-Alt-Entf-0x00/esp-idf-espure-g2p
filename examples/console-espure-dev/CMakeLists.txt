cmake_minimum_required(VERSION 3.16)

get_filename_component(ESPURE_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Prefer standalone central esp-idf-uart-utf8-console repo if present, fallback to in-tree copy
get_filename_component(STANDALONE_CONSOLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../esp-idf-uart-utf8-console" ABSOLUTE)
get_filename_component(IN_TREE_CONSOLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../common/uart_utf8_console" ABSOLUTE)

if(EXISTS "${STANDALONE_CONSOLE_DIR}/CMakeLists.txt")
    set(UTF8_CONSOLE_COMPONENT_DIR "${STANDALONE_CONSOLE_DIR}")
else()
    set(UTF8_CONSOLE_COMPONENT_DIR "${IN_TREE_CONSOLE_DIR}")
endif()

set(EXTRA_COMPONENT_DIRS
    "${ESPURE_COMPONENT_DIR}"
    "${UTF8_CONSOLE_COMPONENT_DIR}"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(console-espure-dev)
