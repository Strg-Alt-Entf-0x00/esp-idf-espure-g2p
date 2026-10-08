# ESP-IDF espure Basic Usage Example

cmake_minimum_required(VERSION 3.16)

# Add parent directory as component (absolute path to esp-idf-espure-g2p)
get_filename_component(ESPURE_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${ESPURE_COMPONENT_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(espure_basic_usage)
