idf_component_register(
    SRCS
        "app_main.c"
        "console_commands.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp-idf-espure-g2p
        uart_utf8_console
        esp-uart-filebridge
        esp_timer
        esp_system
        heap
        driver
        esp_driver_uart
        vfs
)
