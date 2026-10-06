idf_component_register(
    SRCS "esp32_g2p_test.c"
    INCLUDE_DIRS "."
    REQUIRES esp-idf-espure esp_driver_uart
)
