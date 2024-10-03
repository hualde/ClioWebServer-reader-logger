#ifndef LOG_READER_H
#define LOG_READER_H

#include <stdint.h>

// Estructura para almacenar una entrada de log
typedef struct {
    char timestamp[20];
    char message[100];
} LogEntry;

// Función para inicializar el lector de logs
void init_log_reader();

// Función para obtener la última entrada de log
LogEntry get_latest_log();

#endif // LOG_READER_H