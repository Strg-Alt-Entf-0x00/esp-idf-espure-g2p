# ESP-IDF-espure Parity Test Project

cmake_minimum_required(VERSION 3.16)

# Add parent directory as component (absolute path for ESP-IDF 6.0)
get_filename_component(ESPURE_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${ESPURE_COMPONENT_DIR}")

message(STATUS "EXTRA_COMPONENT_DIRS = ${EXTRA_COMPONENT_DIRS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(espure_parity_test)
