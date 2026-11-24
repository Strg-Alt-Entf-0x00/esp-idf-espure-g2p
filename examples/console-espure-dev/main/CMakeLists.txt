idf_component_register(
    SRCS
        "app_main.c"
        "console_commands.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp-idf-espure-g2p
        esp-idf-uart-utf8-console
        esp_timer
        esp_system
        heap
)
