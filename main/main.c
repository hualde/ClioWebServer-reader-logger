#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "CAN.h"

#define EXAMPLE_ESP_WIFI_SSID      "ESP32_CAN_Viewer"
#define EXAMPLE_ESP_WIFI_PASS      ""
#define EXAMPLE_MAX_STA_CONN       4
#define MAX_CAN_MESSAGES 100

static const char *TAG = "wifi softAP";
static httpd_handle_t server = NULL;

static char can_messages[MAX_CAN_MESSAGES][100];
static int message_count = 0;
static int message_index = 0;
static SemaphoreHandle_t can_buffer_mutex;

void add_can_message(const char* message) {
    xSemaphoreTake(can_buffer_mutex, portMAX_DELAY);
    
    // Verificar si el mensaje ya existe en el buffer
    bool message_exists = false;
    for (int i = 0; i < message_count; i++) {
        int index = (message_index - message_count + i + MAX_CAN_MESSAGES) % MAX_CAN_MESSAGES;
        if (strcmp(can_messages[index], message) == 0) {
            message_exists = true;
            break;
        }
    }
    
    // Si el mensaje no existe, agregarlo al buffer
    if (!message_exists) {
        strncpy(can_messages[message_index], message, 99);
        can_messages[message_index][99] = '\0';
        message_index = (message_index + 1) % MAX_CAN_MESSAGES;
        if (message_count < MAX_CAN_MESSAGES) {
            message_count++;
        }
    }
    
    xSemaphoreGive(can_buffer_mutex);
}

char* get_all_can_messages() {
    static char all_messages[MAX_CAN_MESSAGES * 200];
    all_messages[0] = '\0';

    xSemaphoreTake(can_buffer_mutex, portMAX_DELAY);
    for (int i = 0; i < message_count; i++) {
        int index = (message_index - message_count + i + MAX_CAN_MESSAGES) % MAX_CAN_MESSAGES;
        strcat(all_messages, can_messages[index]);
        strcat(all_messages, "<br><br>");
    }
    xSemaphoreGive(can_buffer_mutex);

    return all_messages;
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

static esp_err_t http_server_handler(httpd_req_t *req)
{
    const char* resp_str = "<!DOCTYPE html>"
                           "<html>"
                           "<head>"
                           "<title>ESP32 WebSocket CAN Viewer</title>"
                           "<style>"
                           "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f0f0f0; }"
                           "h1 { color: #333; }"
                           ".message-window { "
                           "    background-color: #fff; "
                           "    border: 1px solid #ddd; "
                           "    border-radius: 5px; "
                           "    height: 400px; "
                           "    overflow-y: auto; "
                           "    padding: 10px; "
                           "    margin-bottom: 20px; "
                           "}"
                           "#can-messages { white-space: pre-wrap; }"
                           "</style>"
                           "<script>"
                           "var socket;"
                           "function initWebSocket() {"
                           "    console.log('Trying to open a WebSocket connection...');"
                           "    socket = new WebSocket('ws://' + window.location.host + '/ws');"
                           "    socket.onopen = function(event) {"
                           "        console.log('WebSocket connection opened');"
                           "        socket.send('Hello Server!');"
                           "    };"
                           "    socket.onmessage = function(event) {"
                           "        console.log('Received message:', event.data);"
                           "        document.getElementById('can-messages').innerHTML = event.data;"
                           "        var messageWindow = document.querySelector('.message-window');"
                           "        messageWindow.scrollTop = messageWindow.scrollHeight;"
                           "    };"
                           "    socket.onerror = function(error) {"
                           "        console.error('WebSocket error:', error);"
                           "    };"
                           "    socket.onclose = function(event) {"
                           "        console.log('WebSocket connection closed');"
                           "        setTimeout(initWebSocket, 2000);"
                           "    };"
                           "}"
                           "function requestLatestMessages() {"
                           "    if (socket && socket.readyState === WebSocket.OPEN) {"
                           "        socket.send('get_messages');"
                           "    }"
                           "}"
                           "window.addEventListener('load', function() {"
                           "    initWebSocket();"
                           "    setInterval(requestLatestMessages, 1000);"
                           "});"
                           "</script>"
                           "</head>"
                           "<body>"
                           "<h1>ESP32 CAN Message Viewer</h1>"
                           "<div class='message-window'>"
                           "    <div id='can-messages'>Waiting for messages...</div>"
                           "</div>"
                           "</body>"
                           "</html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

static esp_err_t websocket_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
    }

    const char* all_messages = get_all_can_messages();
    ws_pkt.payload = (uint8_t*)all_messages;
    ws_pkt.len = strlen(all_messages);

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    
    free(buf);
    return ret;
}

static const httpd_uri_t ws = {
    .uri        = "/ws",
    .method     = HTTP_GET,
    .handler    = websocket_handler,
    .user_ctx   = NULL,
    .is_websocket = true
};

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = http_server_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &ws);
        return server;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
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
            .authmode = WIFI_AUTH_OPEN
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

void can_message_task(void *pvParameters) {
    while (1) {
        const char* latest_message = get_latest_can_message();
        if (strlen(latest_message) > 0) {
            add_can_message(latest_message);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait 100ms before checking for the next message
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    can_buffer_mutex = xSemaphoreCreateMutex();
    if (can_buffer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create CAN buffer mutex");
        return;
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    
    init_can();
    start_can_tasks();

    // Ejecutar empty_task_199 una vez al inicio
    empty_task_199();
    
    server = start_webserver();

    if (server == NULL) {
        ESP_LOGE(TAG, "Failed to start web server. Restarting...");
        esp_restart();
    }

    xTaskCreate(can_message_task, "can_message_task", 2048, NULL, 5, NULL);
}