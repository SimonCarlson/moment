#pragma once
#include <cstdint>
typedef void* SemaphoreHandle_t;
#define portMAX_DELAY 0xFFFFFFFFUL
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)
inline void vTaskDelay(uint32_t) {}
