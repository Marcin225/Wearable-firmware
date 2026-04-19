#ifndef TASKS_H
#define TASKS_H

#include "SystemContext.h"

void vCollectAndFilterDataTask(void *pvParameters);
void vCalculateVitalsTask(void *pvParameters);

#endif