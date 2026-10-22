set(srcs
        "src/espure_api.c"
        "src/espure_dictionary.c"
        "src/espure_dict_binary.c"
        "src/espure_memory.c"
        "src/espure_morph.c"
        "src/espure_normalize.c"
        "src/espure_phoneme.c"
        "src/espure_phoneme_parser.c"
        "src/espure_phoneme_program.c"
        "src/espure_phoneme_program_integration.c"
        "src/espure_phonology.c"
        "src/espure_phsource_parser.c"
        "src/espure_render.c"
        "src/espure_rules.c"
        "src/espure_stress.c"
)

if(CONFIG_ESPURE_LOCALE_DE_DE)
    message(STATUS "espure: Adding German (de-DE) locale files")
    list(APPEND srcs 
        "lang/de_DE/de_DE_rules_data.c"
        "lang/de_DE/de_DE_morphology_data.c"
        "lang/de_DE/de_DE_morph_trie.c"
        "lang/de_DE/de_DE_ipa.c"
        "lang/de_DE/de_DE_phonemes_data.c"
        "lang/de_DE/de_DE_normalize.c"
    )
    
    if(CONFIG_ESPURE_DICT_HUFFMAN_TRIE)
        list(APPEND srcs "lang/de_DE/de_DE_dict_compressed.c")
        message(STATUS "espure: Using compressed dictionary (Huffman Trie)")
    else()
        list(APPEND srcs "lang/de_DE/de_DE_dict.c")
        message(STATUS "espure: Using binary array dictionary")
    endif()
endif()

if(CONFIG_ESPURE_LOCALE_EN_GB)
    message(STATUS "espure: Adding English (en-GB) pilot files")
    message(WARNING "espure: en-GB is an architecture pilot with limited vocabulary")
    list(APPEND srcs
        "lang/en_GB/en_GB_dict.c"
        "lang/en_GB/en_GB_ipa.c"
        "lang/en_GB/en_GB_phonemes_data.c"
        "lang/en_GB/en_GB_normalize.c"
    )
endif()

idf_component_register(
    SRCS
        ${srcs}
    INCLUDE_DIRS 
        "include"
    PRIV_INCLUDE_DIRS
        "src"
        "lang/de_DE"
    REQUIRES
        esp_timer
)
