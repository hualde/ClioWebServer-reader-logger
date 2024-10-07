// CAN.h
#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"

void init_can(void);
void start_can_tasks(void);
const char* get_latest_can_message(void);

esp_err_t send_can_message_id_199(void);

#endif // CAN_H