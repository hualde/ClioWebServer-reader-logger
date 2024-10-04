#include "CAN.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TAG "TWAI_EXAMPLE"
#define RESTART_INTERVAL_US (1800000000) // 30 minutes in microseconds
#define MAX_CONSECUTIVE_ERRORS 5
#define FRAMES_BEFORE_COOLDOWN 60
#define COOLDOWN_TIME_MS 1000
#define BASE_RETRY_INTERVAL_MS 100
#define MAX_RETRY_INTERVAL_MS 5000
#define TRANSMIT_INTERVAL_MS 1000 // 1 seconds between transmissions
#define ERROR_RECOVERY_DELAY_MS 10000 // 10 seconds delay for error recovery
#define BUFFER_CHECK_INTERVAL_MS 30000 // Check buffer status every 30 seconds
#define TX_QUEUE_LEN 32 // Increased TX queue length
#define RX_QUEUE_LEN 32 // Increased RX queue length
#define MAX_REINIT_ATTEMPTS 3 // Maximum number of reinitialization attempts

static char latest_message[50];
static int consecutive_errors = 0;
static int frames_sent = 0;
static SemaphoreHandle_t twai_mutex;
static QueueHandle_t tx_task_queue;
static int reinit_attempts = 0;

static esp_err_t init_twai(void) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_18, GPIO_NUM_19, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = TX_QUEUE_LEN;
    g_config.rx_queue_len = RX_QUEUE_LEN;
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

static esp_err_t reinit_twai(void) {
    ESP_LOGI(TAG, "Reinitializing TWAI driver");
    twai_stop();
    twai_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 1 second before reinitializing
    esp_err_t result = init_twai();
    if (result == ESP_OK) {
        reinit_attempts = 0; // Reset the reinit attempts counter on success
        ESP_LOGI(TAG, "TWAI driver reinitialized successfully");
    } else {
        reinit_attempts++;
        ESP_LOGE(TAG, "Failed to reinitialize TWAI driver: %s (Attempt %d/%d)", 
                 esp_err_to_name(result), reinit_attempts, MAX_REINIT_ATTEMPTS);
        if (reinit_attempts >= MAX_REINIT_ATTEMPTS) {
            ESP_LOGE(TAG, "Max reinit attempts reached. Restarting system...");
            esp_restart();
        }
    }
    return result;
}

static void check_twai_status(void) {
    twai_status_info_t status_info;
    esp_err_t res = twai_get_status_info(&status_info);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "TWAI Status: %s, RX msgs: %lu, TX msgs: %lu, RX errors: %lu, TX errors: %lu, Bus errors: %lu",
                 status_info.state == TWAI_STATE_RUNNING ? "RUNNING" : "STOPPED",
                 status_info.msgs_to_rx, status_info.msgs_to_tx,
                 status_info.rx_error_counter, status_info.tx_error_counter,
                 status_info.bus_error_count);
        
        if (status_info.state == TWAI_STATE_BUS_OFF) {
            ESP_LOGE(TAG, "TWAI controller is in bus-off state. Initiating recovery...");
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for recovery to complete
            if (reinit_twai() != ESP_OK) {
                ESP_LOGE(TAG, "Failed to recover from bus-off state");
            }
        }

        // Check if buffers are near full and clear if necessary
        if (status_info.msgs_to_tx > TX_QUEUE_LEN * 0.8 || status_info.msgs_to_rx > RX_QUEUE_LEN * 0.8) {
            ESP_LOGW(TAG, "Buffers are near full. Clearing...");
            twai_clear_transmit_queue();
            twai_clear_receive_queue();
        }
    } else {
        ESP_LOGE(TAG, "Failed to get TWAI status: %s", esp_err_to_name(res));
    }
}

static void error_recovery_procedure(void) {
    ESP_LOGI(TAG, "Starting error recovery procedure");
    xSemaphoreTake(twai_mutex, portMAX_DELAY);
    
    if (reinit_twai() != ESP_OK) {
        ESP_LOGE(TAG, "Error recovery failed");
    } else {
        ESP_LOGI(TAG, "Error recovery procedure completed successfully");
    }
    
    xSemaphoreGive(twai_mutex);
}

static void twai_transmit_task(void *arg) {
    twai_message_t message;
    message.identifier = 0x742;
    message.extd = 0;
    message.rtr = 0;
    message.data_length_code = 8;

    message.data[0] = 0x03;
    message.data[1] = 0x14;
    message.data[2] = 0xFF;
    message.data[3] = 0x00;
    message.data[4] = 0x00;
    message.data[5] = 0x00;
    message.data[6] = 0x00;

    int64_t start_time = esp_timer_get_time();
    int64_t current_time;
    int retry_interval = BASE_RETRY_INTERVAL_MS;
    int64_t last_buffer_check = 0;

    while (1) {
        current_time = esp_timer_get_time();
        if (current_time - start_time >= RESTART_INTERVAL_US) {
            ESP_LOGI(TAG, "Reiniciando el dispositivo después de 2 minutos...");
            esp_restart();
        }

        // Periodic buffer check
        if (current_time - last_buffer_check >= BUFFER_CHECK_INTERVAL_MS * 1000) {
            check_twai_status();
            last_buffer_check = current_time;
        }

        message.data[7] = (uint8_t)(rand() % 256);

        xSemaphoreTake(twai_mutex, portMAX_DELAY);
        esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
        xSemaphoreGive(twai_mutex);

        if (result == ESP_OK) {
            snprintf(latest_message, sizeof(latest_message), "Trama CAN enviada. Último byte: 0x%02X", message.data[7]);
            ESP_LOGI(TAG, "%s", latest_message);
            consecutive_errors = 0;
            frames_sent++;
            retry_interval = BASE_RETRY_INTERVAL_MS;

            if (frames_sent >= FRAMES_BEFORE_COOLDOWN) {
                ESP_LOGI(TAG, "Iniciando período de enfriamiento...");
                vTaskDelay(pdMS_TO_TICKS(COOLDOWN_TIME_MS));
                frames_sent = 0;
                check_twai_status(); // Check TWAI status after cooldown
            }
        } else {
            snprintf(latest_message, sizeof(latest_message), "Error al enviar la trama CAN: %s", esp_err_to_name(result));
            ESP_LOGE(TAG, "%s", latest_message);
            
            consecutive_errors++;
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                ESP_LOGI(TAG, "Demasiados errores consecutivos. Iniciando procedimiento de recuperación...");
                error_recovery_procedure();
                consecutive_errors = 0;
                frames_sent = 0;
            } else {
                // Implement exponential backoff
                vTaskDelay(pdMS_TO_TICKS(retry_interval));
                retry_interval = (retry_interval * 2 < MAX_RETRY_INTERVAL_MS) ? retry_interval * 2 : MAX_RETRY_INTERVAL_MS;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TRANSMIT_INTERVAL_MS));
    }

    vTaskDelete(NULL);
}

static void twai_receive_task(void *arg) {
    twai_message_t rx_message;

    while (1) {
        xSemaphoreTake(twai_mutex, portMAX_DELAY);
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(100));
        xSemaphoreGive(twai_mutex);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Message received - ID: 0x%lx, DLC: %d, Data: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
                     rx_message.identifier,
                     rx_message.data_length_code,
                     rx_message.data[0], rx_message.data[1], rx_message.data[2], rx_message.data[3],
                     rx_message.data[4], rx_message.data[5], rx_message.data[6], rx_message.data[7]);
        } else if (result != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Failed to receive message: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent tight looping
    }

    vTaskDelete(NULL);
}

void init_can(void) {
    srand(time(NULL));

    twai_mutex = xSemaphoreCreateMutex();
    if (twai_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create TWAI mutex");
        esp_restart();
    }

    tx_task_queue = xQueueCreate(TX_QUEUE_LEN, sizeof(twai_message_t));
    if (tx_task_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TX task queue");
        esp_restart();
    }

    if (init_twai() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TWAI. Restarting...");
        esp_restart();
    }
}

void start_can_tasks(void) {
    xTaskCreate(twai_transmit_task, "twai_transmit_task", 4096, NULL, 5, NULL);
    xTaskCreate(twai_receive_task, "twai_receive_task", 4096, NULL, 5, NULL);
}

const char* get_latest_can_message(void) {
    return latest_message;
}