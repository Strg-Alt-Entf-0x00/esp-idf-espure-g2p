cmake_minimum_required(VERSION 3.16)
project(espure_host_test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(CTest)

option(ESPURE_HOST_TEST_EN_US "Include the bounded en-US architecture pilot" ON)
option(ESPURE_HOST_TEST_HUFFMAN "Use the compressed German dictionary" OFF)

if(MSVC)
    add_compile_options(/bigobj)
endif()

add_compile_definitions(ESPURE_HOST_TEST CONFIG_ESPURE_LOCALE_DE_DE)
if(ESPURE_HOST_TEST_EN_US)
    add_compile_definitions(CONFIG_ESPURE_LOCALE_EN_US)
endif()
if(ESPURE_HOST_TEST_HUFFMAN)
    add_compile_definitions(CONFIG_ESPURE_DICT_HUFFMAN_TRIE)
endif()

set(ESPURE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

include_directories(
    "${ESPURE_ROOT}/include"
    "${ESPURE_ROOT}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

file(GLOB ESPURE_SOURCES
    "${ESPURE_ROOT}/src/*.c"
    "${ESPURE_ROOT}/lang/de_DE/*.c"
)

if(ESPURE_HOST_TEST_EN_US)
    file(GLOB ESPURE_EN_US_SOURCES
        "${ESPURE_ROOT}/lang/en_US/*.c"
    )
    list(APPEND ESPURE_SOURCES ${ESPURE_EN_US_SOURCES})
endif()

if(ESPURE_HOST_TEST_HUFFMAN)
    list(REMOVE_ITEM ESPURE_SOURCES
        "${ESPURE_ROOT}/lang/de_DE/de_DE_dict.c"
    )
else()
    list(REMOVE_ITEM ESPURE_SOURCES
        "${ESPURE_ROOT}/lang/de_DE/de_DE_dict_compressed.c"
    )
endif()

add_library(espure STATIC ${ESPURE_SOURCES})

function(add_espure_test target source test_name)
    add_executable(${target} ${source}
        "${CMAKE_CURRENT_SOURCE_DIR}/stubs/libc_stubs.c")
    target_link_libraries(${target} PRIVATE espure)
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

add_espure_test(test_dictionary_case
    test_dictionary_case.c dictionary_preserves_case_variants)
add_espure_test(test_runtime_bounds
    test_runtime_bounds.c runtime_bounds_and_german_ipa_rendering)
add_espure_test(test_memory
    test_memory.c memory_allocation_fallbacks)
add_espure_test(test_phonological_rules
    test_phonological_rules.c german_phonological_rules)
add_espure_test(test_normalization
    test_normalization.c text_normalization)
add_espure_test(test_stream
    test_stream.c structured_phoneme_stream)

if(ESPURE_HOST_TEST_EN_US)
    add_espure_test(test_language_pilot
        test_language_pilot.c multilingual_architecture_pilot)
endif()

add_library(espure_shared SHARED ${ESPURE_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/stubs/libc_stubs.c")
set_target_properties(espure_shared PROPERTIES
    OUTPUT_NAME "espure_shared"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
