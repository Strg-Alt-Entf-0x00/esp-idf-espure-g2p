/**
 * @file app_main.c
 * @brief Entry point for ESPure G2P Developer Console
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void espure_console_start(void);

void app_main(void)
{
    // Start ESPure G2P console
    espure_console_start();
}

