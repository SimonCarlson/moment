#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (void*)1; }
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return (void*)1; }
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return (void*)1; }
inline int xSemaphoreTake(SemaphoreHandle_t, uint32_t) { return pdTRUE; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, uint32_t) { return pdTRUE; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t) { return pdTRUE; }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
