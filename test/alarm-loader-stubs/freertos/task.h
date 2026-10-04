#pragma once
#include "FreeRTOS.h"
static inline uint32_t xTaskGetTickCount(void){return 1;}
static inline void vTaskDelay(uint32_t n){(void)n;}
