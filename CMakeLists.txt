# ============================================================================
# Dictionary Selection: Dynamically compile only enabled languages
# ============================================================================
set(DICT_SRCS "")

# Find all dictionary source files automatically
file(GLOB ALL_DICT_FILES "data/dict/*_dict.c")

foreach(dict_file IN LISTS ALL_DICT_FILES)
    # Extract the language tag (e.g., 'de_DE' from 'de_DE_dict.c')
    get_filename_component(filename ${dict_file} NAME)
    string(REGEX REPLACE "_dict\.c$" "" lang_lower ${filename})
    
    string(TOUPPER "${lang_lower}" lang_upper)

    # Append to sources if the user enabled it in menuconfig
    if(CONFIG_ESPURE_LANG_${lang_upper})
        list(APPEND DICT_SRCS ${dict_file})
    endif()
endforeach()

# Include directories
idf_component_register(
    SRCS 
"src/espure_api.c"
        "src/espure_dictionary.c"
        "src/espure_dict_binary.c"
        "src/espure_phoneme.c"
        "src/espure_phoneme_program.c"
        "src/espure_phoneme_program_integration.c"
        "src/espure_phoneme_parser.c"
        "src/espure_phsource_parser.c"
        "src/espure_phsource_data.c"
        "src/espure_rules.c"
        "src/de_DE_simple_rules.c"
        "src/de_DE_rules_data.c"
        "src/espure_render.c"
        "src/espure_stress.c"
        "src/espure_memory.c"
        ${DICT_SRCS}
    INCLUDE_DIRS 
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
    esp_psram          # PSRAM (2.5MB language data)
    esp_hw_support     # Hardware support
    heap               # Memory management
    driver             # Driver support
    esp_system         # System support
    esp_timer          # High-resolution profiling
)

# ESP32-P4 specific optimizations
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3                          # Maximum optimization
    -ffast-math                  # Fast math operations
    -funroll-loops               # Loop unrolling
    -fomit-frame-pointer         # Remove frame pointer
    -march=rv32imafc_zicsr_zifencei  # RISC-V architecture
    -Wall -Wextra
)

# ============================================================================
# Binary Dictionary Converter (Optional)
# ============================================================================
if(CONFIG_ESPURE_USE_BINARY_DICT)
    # Find Python interpreter
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    
    # Create binary dictionary directory
    set(DICT_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/dict_bin")
    file(MAKE_DIRECTORY ${DICT_BIN_DIR})
    
    # Build compression flag
    if(CONFIG_ESPURE_DICT_COMPRESSION)
        set(COMPRESSION_FLAG "-c")
    else()
        set(COMPRESSION_FLAG "")
    endif()
    
    # Convert dictionaries at build time
    add_custom_target(espure_convert_dicts
        COMMAND ${Python3_EXECUTABLE} 
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/dict_to_binary.py
                ${CMAKE_CURRENT_SOURCE_DIR}/data/dict
                -o ${DICT_BIN_DIR}
                ${COMPRESSION_FLAG}
                --all
        COMMENT "Converting dictionaries to binary format..."
        VERBATIM
    )
    
    # Add dependency to component
    add_dependencies(${COMPONENT_LIB} espure_convert_dicts)
    
    # TODO: Embed binary dictionaries into SPIFFS partition
    # This would be done in main CMakeLists.txt with spiffs_create_partition_image()
endif()

# TODO Phase 8: Embed language data
# target_add_binary_data(${COMPONENT_LIB} 
#     "data/phsource/phonemes" TEXT
# )
