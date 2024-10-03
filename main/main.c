#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_wifi_types.h"

#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

#define TAG "TWAI_EXAMPLE"
#define TARGET_ID_1 0x765  // ID objetivo para filtrar las tramas CAN del vehículo
#define TARGET_ID_2 0x762  // ID objetivo para filtrar las tramas CAN de la columna
#define TX_GPIO_NUM 18   // GPIO para transmisión CAN
#define RX_GPIO_NUM 19   // GPIO para recepción CAN
#define RX_QUEUE_SIZE 64 // Tamaño aumentado del buffer de recepción
#define VIN_LENGTH 17    // Longitud del VIN

#define VIN_VEHICLE_BIT BIT0
#define VIN_COLUMN_BIT BIT1
#define STOP_TASKS_BIT BIT2

#define EXAMPLE_ESP_WIFI_SSID      "ESP32_VIN_AP"
#define EXAMPLE_ESP_WIFI_PASS      "password123"
#define EXAMPLE_MAX_STA_CONN       4

uint8_t stored_bytes_vehicle[VIN_LENGTH];
uint8_t stored_bytes_column[VIN_LENGTH];
int stored_bytes_count_vehicle = 0;
int stored_bytes_count_column = 0;
char vin_vehiculo[VIN_LENGTH + 1]; // +1 para el carácter nulo al final
char vin_columna[VIN_LENGTH + 1]; // +1 para el carácter nulo al final

EventGroupHandle_t vin_event_group;
TimerHandle_t restart_timer;
static httpd_handle_t server = NULL;

const char index_html[] = R"rawliteral(
const char index_html[] = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 VIN Display</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding-top: 30px; }
    table { margin-left: auto; margin-right: auto; }
    td { padding: 8px; }
  </style>
</head>
<body>
  <h1>ESP32 VIN Display</h1>
  <table>
    <tr><td>VIN del vehículo:</td><td><span id="vin_vehiculo">%s</span></td></tr>
    <tr><td>VIN de la columna:</td><td><span id="vin_columna">%s</span></td></tr>
  </table>
</body>
</html>
)rawliteral";

void send_can_frame(uint32_t id, uint8_t *data, uint8_t dlc) {
    if (xEventGroupGetBits(vin_event_group) & STOP_TASKS_BIT) {
        return;  // No enviar si se ha señalizado la detención
    }

    twai_message_t message;
    message.identifier = id;
    message.extd = 0;  // Frame estándar (11 bits)
    message.rtr = 0;   // No es una solicitud de transmisión remota
    message.data_length_code = dlc;
    memcpy(message.data, data, dlc);

    esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error al enviar trama CAN: ID=0x%03" PRIx32 " Error: %s", id, esp_err_to_name(result));
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
}

void empty_task_199(void *pvParameters) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    send_can_frame(0x199, data, 8);
    ESP_LOGI(TAG, "Tarea vacía con ID 199 ejecutada");
    vTaskDelete(NULL);
}

void communication_task(void *pvParameters) {
    uint8_t frame1[] = {0x02, 0x10, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame2[] = {0x02, 0x21, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame3[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame4[] = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame5[] = {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame6[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame7[] = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame8[] = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame9[] = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame10[] = {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame11[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame12[] = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame13[] = {0x02, 0x21, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame14[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame15[] = {0x02, 0x21, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame16[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8_t frame17[] = {0x02, 0x10, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame18[] = {0x02, 0x21, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame19[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame20[] = {0x02, 0x3E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame21[] = {0x02, 0x21, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t frame22[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    while (1) {
        EventBits_t bits = xEventGroupGetBits(vin_event_group);
        if (bits & STOP_TASKS_BIT) {
            ESP_LOGI(TAG, "Deteniendo la tarea de comunicacion.");
            vTaskDelete(NULL);
        }

        if (!(bits & VIN_VEHICLE_BIT)) {
            send_can_frame(0x745, frame1, 8);
            send_can_frame(0x745, frame2, 8);
            send_can_frame(0x745, frame3, 8);
            send_can_frame(0x745, frame4, 8);
            send_can_frame(0x745, frame5, 8);
            send_can_frame(0x745, frame6, 8);
            send_can_frame(0x745, frame7, 8);
            send_can_frame(0x745, frame8, 8);
            send_can_frame(0x745, frame9, 8);
            send_can_frame(0x745, frame10, 8);
            send_can_frame(0x745, frame11, 8);
            send_can_frame(0x745, frame12, 8);
            send_can_frame(0x745, frame13, 8);
            send_can_frame(0x745, frame14, 8);
            send_can_frame(0x745, frame15, 8);
            send_can_frame(0x745, frame16, 8);
        } else if (!(bits & VIN_COLUMN_BIT)) {
            send_can_frame(0x742, frame17, 8);
            send_can_frame(0x742, frame18, 8);
            send_can_frame(0x742, frame19, 8);
            send_can_frame(0x742, frame20, 8);
            send_can_frame(0x742, frame21, 8);
            send_can_frame(0x742, frame22, 8);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void store_bytes(uint8_t *stored_bytes, int *stored_bytes_count, uint8_t *data, int start, int length) {
    for (int i = 0; i < length && *stored_bytes_count < VIN_LENGTH; i++) {
        stored_bytes[(*stored_bytes_count)++] = data[start + i];
    }
}

bool validate_vin(const char *vin, bool is_vehicle) {
    if (strlen(vin) != VIN_LENGTH) {
        return false;
    }
    
    for (int i = 0; i < VIN_LENGTH; i++) {
        if (vin[i] == '\0' || vin[i] == ' ') {
            return false;
        }
    }
    
    if (is_vehicle && strncmp(vin, "VF1", 3) != 0) {
        return false;
    }
    
    return true;
}

void receive_task(void *pvParameters) {
    while (1) {
        EventBits_t bits = xEventGroupGetBits(vin_event_group);
        if (bits & STOP_TASKS_BIT) {
            ESP_LOGI(TAG, "Deteniendo la tarea de recepcion.");
            vTaskDelete(NULL);
        }

        twai_message_t message;
        esp_err_t result = twai_receive(&message, pdMS_TO_TICKS(100));
        if (result == ESP_OK) {
            if (message.identifier == TARGET_ID_1 || message.identifier == TARGET_ID_2) {
                uint8_t *target_stored_bytes;
                int *target_stored_bytes_count;
                char *target_VIN;
                EventBits_t vin_bit;
                bool is_vehicle;

                if (message.identifier == TARGET_ID_1 && !(bits & VIN_VEHICLE_BIT)) {
                    target_stored_bytes = stored_bytes_vehicle;
                    target_stored_bytes_count = &stored_bytes_count_vehicle;
                    target_VIN = vin_vehiculo;
                    vin_bit = VIN_VEHICLE_BIT;
                    is_vehicle = true;
                } else if (message.identifier == TARGET_ID_2 && !(bits & VIN_COLUMN_BIT)) {
                    target_stored_bytes = stored_bytes_column;
                    target_stored_bytes_count = &stored_bytes_count_column;
                    target_VIN = vin_columna;
                    vin_bit = VIN_COLUMN_BIT;
                    is_vehicle = false;
                } else {
                    continue;
                }

                if (message.data[0] == 0x10 && message.data_length_code >= 8) {
                    store_bytes(target_stored_bytes, target_stored_bytes_count, message.data, 4, 4);
                } 
                else if (message.data[0] == 0x21 && message.data_length_code == 8) {
                    store_bytes(target_stored_bytes, target_stored_bytes_count, message.data, 1, 7);
                } 
                else if (message.data[0] == 0x22 && message.data_length_code == 8) {
                    store_bytes(target_stored_bytes, target_stored_bytes_count, message.data, 1, 6);
                }

                if (*target_stored_bytes_count == VIN_LENGTH) {
                    for (int i = 0; i < VIN_LENGTH; i++) {
                        target_VIN[i] = (char)target_stored_bytes[i];
                    }
                    target_VIN[VIN_LENGTH] = '\0';

                    if (validate_vin(target_VIN, is_vehicle)) {
                        if (is_vehicle) {
                            ESP_LOGI(TAG, "VIN del vehiculo valido: %s", target_VIN);
                        } else {
                            ESP_LOGI(TAG, "VIN de la columna valido: %s", target_VIN);
                        }
                        xEventGroupSetBits(vin_event_group, vin_bit);
                    } else {
                        if (is_vehicle) {
                            ESP_LOGW(TAG, "VIN del vehiculo no valido: %s", target_VIN);
                        } else {
                            ESP_LOGW(TAG, "VIN de la columna no valido: %s", target_VIN);
                        }
                        *target_stored_bytes_count = 0;
                        continue;
                    }
                    
                    *target_stored_bytes_count = 0;
                }
            }
        } else if (result != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Error al recibir trama CAN: %s", esp_err_to_name(result));
        }
    }
}

void set_vin_task(void *pvParameters) {
    uint8_t data1[] = {0x02, 0x10, 0xC0, 0x30, 0x30, 0x30, 0x30, 0x30};
    send_can_frame(0x742, data1, 8);

    uint8_t data2[8] = {0x10, 0x15, 0x3B, 0x81, 0, 0, 0, 0};
    memcpy(data2 + 4, vin_vehiculo, 4);
    send_can_frame(0x742, data2, 8);

    uint8_t data3[8] = {0x21, 0, 0, 0, 0, 0, 0, 0};
    memcpy(data3 + 1, vin_vehiculo + 4, 7);
    send_can_frame(0x742, data3, 8);

    uint8_t data4[8] = {0x22, 0, 0, 0, 0, 0, 0, 0};
    memcpy(data4 + 1, vin_vehiculo + 11, 6);
    data4[7] = 0x49;
    send_can_frame(0x742, data4, 8);

    uint8_t data5[] = {0x23, 0xE7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    send_can_frame(0x742, data5, 8);

    uint8_t data6[] = {0x02, 0x10, 0xFA, 0x30, 0x30, 0x30, 0x30, 0x30};
    send_can_frame(0x742, data6, 8);

    ESP_LOGI(TAG, "VIN establecido");
    vTaskDelete(NULL);
}

void set_immo_task(void *pvParameters) {
    uint8_t data1[] = {0x02, 0x10, 0xC0, 0x08, 0x00, 0x00, 0x00, 0x08};
    send_can_frame(0x742, data1, 8);

    uint8_t data2[] = {0x02, 0x10, 0xFB, 0x08, 0x00, 0x00, 0x00, 0x08};
    send_can_frame(0x742, data2, 8);

    uint8_t data3[] = {0x02, 0x3B, 0x05, 0x08, 0x00, 0x00, 0x00, 0x08};
    send_can_frame(0x742, data3, 8);

    uint8_t data4[] = {0x02, 0x10, 0xFA, 0x0D, 0xB6, 0x01, 0xFF, 0xFF};
    send_can_frame(0x742, data4, 8);

    ESP_LOGI(TAG, "IMMO programado");
    vTaskDelete(NULL);
}

void clear_dtc_task(void *pvParameters) {
    uint8_t data[] = {0x03, 0x14, 0xFF, 0x00, 0xB6, 0x01, 0xFF, 0xFF};
    send_can_frame(0x742, data, 8);

    ESP_LOGI(TAG, "Senal de borrado de fallos enviada");
    vTaskDelete(NULL);
}

void restart_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Reiniciando el microcontrolador...");
    esp_restart();
}

esp_err_t root_handler(httpd_req_t *req)
{
    char response[sizeof(index_html) + 2*VIN_LENGTH];
    snprintf(response, sizeof(response), index_html, vin_vehiculo, vin_columna);
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &root);
    }
    return server;
}

void stop_webserver(httpd_handle_t server)
{
    if (server) {
        httpd_stop(server);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = 1,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, 1);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();

    server = start_webserver();

    vin_event_group = xEventGroupCreate();

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = RX_QUEUE_SIZE;
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        ESP_LOGI(TAG, "Driver TWAI instalado");
    } else {
        ESP_LOGE(TAG, "Error al instalar el driver TWAI");
        return;
    }

    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "Driver TWAI iniciado");
    } else {
        ESP_LOGE(TAG, "Error al iniciar el driver TWAI");
        return;
    }

    xTaskCreate(empty_task_199, "empty_task_199", 2048, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(1000));

    xTaskCreate(communication_task, "communication_task", 2048, NULL, 5, NULL);
    xTaskCreate(receive_task, "receive_task", 2048, NULL, 6, NULL);

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(vin_event_group, VIN_VEHICLE_BIT | VIN_COLUMN_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        if ((bits & (VIN_VEHICLE_BIT | VIN_COLUMN_BIT)) == (VIN_VEHICLE_BIT | VIN_COLUMN_BIT)) {
            ESP_LOGI(TAG, "VIN del vehiculo: %s", vin_vehiculo);
            ESP_LOGI(TAG, "VIN de la columna: %s", vin_columna);
            ESP_LOGI(TAG, "Ambos VINs obtenidos.");
            
            if (strcmp(vin_vehiculo, vin_columna) == 0) {
                ESP_LOGI(TAG, "Los VINs son iguales");
                
                restart_timer = xTimerCreate("RestartTimer", pdMS_TO_TICKS(3600000), pdTRUE, 0, restart_timer_callback);
                
                if (restart_timer != NULL) {
                    xTimerStart(restart_timer, 0);
                    
                    while (1) {
                        xTaskCreate(clear_dtc_task, "clear_dtc_task", 2048, NULL, 5, NULL);
                        vTaskDelay(pdMS_TO_TICKS(30000));
                    }
                } else {
                    ESP_LOGE(TAG, "No se pudo crear el temporizador de reinicio");
                }
            } else {
                ESP_LOGI(TAG, "Los VINs son diferentes");
                
                xTaskCreate(set_vin_task, "set_vin_task", 2048, NULL, 5, NULL);
                
                vTaskDelay(pdMS_TO_TICKS(5000));
                
                xTaskCreate(set_immo_task, "set_immo_task", 2048, NULL, 5, NULL);
                
                vTaskDelay(pdMS_TO_TICKS(5000));
                
                xEventGroupSetBits(vin_event_group, STOP_TASKS_BIT);
                
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (server) {
            stop_webserver(server);
            server = start_webserver();
        }
    }

    ESP_LOGI(TAG, "Deteniendo el driver TWAI...");
    if (twai_stop() == ESP_OK) {
        ESP_LOGI(TAG, "Driver TWAI detenido");
    } else {
        ESP_LOGE(TAG, "Error al detener el driver TWAI");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (twai_driver_uninstall() == ESP_OK) {
        ESP_LOGI(TAG, "Driver TWAI desinstalado");
    } else {
        ESP_LOGE(TAG, "Error al desinstalar el driver TWAI");
    }

    stop_webserver(server);
    vEventGroupDelete(vin_event_group);
    ESP_LOGI(TAG, "Programa finalizado");
}