#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern TaskHandle_t CollectAndFilterTaskHandle;

void vCollectAndFilterDataTask(void *pvParameters);
void vCalculateVitalsTask(void *pvParameters);
void IRAM_ATTR max30102ISR();

#endif