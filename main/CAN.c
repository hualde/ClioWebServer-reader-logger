#include "CAN.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

#define TAG "TWAI_EXAMPLE"
#define RX_TASK_INTERVAL_MS 10
#define BUFFER_SIZE 100

static char latest_message[BUFFER_SIZE];
static SemaphoreHandle_t twai_mutex;

static esp_err_t init_twai(void) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_18, GPIO_NUM_19, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(result));
        return result;
    }

    result = twai_start();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(result));
        return result;
    }

    ESP_LOGI(TAG, "TWAI driver installed and started successfully");
    return ESP_OK;
}

static void twai_receive_task(void *arg) {
    twai_message_t rx_message;

    while (1) {
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(RX_TASK_INTERVAL_MS));
        if (result == ESP_OK) {
            xSemaphoreTake(twai_mutex, portMAX_DELAY);
            snprintf(latest_message, BUFFER_SIZE, "ID: 0x%lx, DLC: %d, Data: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
                     rx_message.identifier,
                     rx_message.data_length_code,
                     rx_message.data[0], rx_message.data[1], rx_message.data[2], rx_message.data[3],
                     rx_message.data[4], rx_message.data[5], rx_message.data[6], rx_message.data[7]);
            xSemaphoreGive(twai_mutex);
            ESP_LOGI(TAG, "Message received - %s", latest_message);
        } else if (result != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Failed to receive message: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(RX_TASK_INTERVAL_MS));
    }
}

void init_can(void) {
    twai_mutex = xSemaphoreCreateMutex();
    if (twai_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create TWAI mutex");
        esp_restart();
    }

    if (init_twai() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TWAI. Restarting...");
        esp_restart();
    }
}

void start_can_tasks(void) {
    xTaskCreate(twai_receive_task, "twai_receive_task", 4096, NULL, 5, NULL);
}

const char* get_latest_can_message(void) {
    static char message_copy[BUFFER_SIZE];
    xSemaphoreTake(twai_mutex, portMAX_DELAY);
    strncpy(message_copy, latest_message, BUFFER_SIZE);
    xSemaphoreGive(twai_mutex);
    return message_copy;
}

void empty_task_199(void) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    send_can_frame(0x199, data, 8);
    ESP_LOGI(TAG, "Tarea vacía con ID 199 ejecutada");
}