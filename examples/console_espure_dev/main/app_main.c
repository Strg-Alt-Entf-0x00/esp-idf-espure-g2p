#include "esp_idf_uart_filebridge.h"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "espure_console_dev";

void espure_console_start(void);

static void mount_sd_task(void *arg)
{
    (void)arg;

    esp_err_t err = esp_idf_uart_filebridge_mount_sd();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Filebridge SD storage is ready at /sd");
    } else {
        ESP_LOGE(TAG, "Filebridge SD mount failed: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_idf_uart_filebridge_config_t filebridge_config =
        ESP_IDF_UART_FILEBRIDGE_CONFIG_DEFAULT();
    filebridge_config.uart_num = UART_NUM_1;
    filebridge_config.mount_sd_own = true;

    esp_err_t err = esp_idf_uart_filebridge_init(&filebridge_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART filebridge initialization failed: %s",
                 esp_err_to_name(err));
    } else {
        BaseType_t task_created = xTaskCreate(
            mount_sd_task,
            "filebridge_sd_mount",
            8192,
            NULL,
            5,
            NULL);
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Could not create SD mount task");
        }

        ESP_LOGI(TAG,
                 "UART filebridge started on UART1; host adapter is expected on COM13");
    }

    espure_console_start();
}
