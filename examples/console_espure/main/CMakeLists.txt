idf_component_register(
    SRCS "console_commands.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp-idf-espure-g2p
        uart_utf8_console
        esp_timer
        esp_system
        heap
        driver
        esp_driver_uart
        vfs
)
