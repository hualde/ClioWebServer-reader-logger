#include "log_reader.h"
#include <string.h>
#include <time.h>
#include <stdio.h>  // Añadimos esta línea para declarar snprintf
#include "esp_log.h"

static const char *TAG = "log_reader";
static LogEntry latest_log;

void init_log_reader() {
    ESP_LOGI(TAG, "Initializing log reader");
    strcpy(latest_log.timestamp, "0000-00-00 00:00:00");
    strcpy(latest_log.message, "Log reader initialized");
}

LogEntry get_latest_log() {
    // Aquí es donde normalmente leerías el archivo de log
    // Por ahora, simularemos la lectura generando una entrada de log
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(latest_log.timestamp, sizeof(latest_log.timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    snprintf(latest_log.message, sizeof(latest_log.message), "Log entry at %s", latest_log.timestamp);

    ESP_LOGI(TAG, "New log entry: %s - %s", latest_log.timestamp, latest_log.message);

    return latest_log;
}