#ifndef TASKS_H
#define TASKS_H

#include "SystemContext.h"

extern TaskHandle_t CollectAndFilterTaskHandle;

void vCollectAndFilterDataTask(void *pvParameters);
void vCalculateVitalsTask(void *pvParameters);
void IRAM_ATTR max30102ISR();

#endif