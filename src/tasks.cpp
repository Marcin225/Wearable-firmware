#include "tasks.h"

void vCollectAndFilterDataTask(void *pvParameters) {
    int32_t medRed[2] = {0, 0};
    int32_t medIr[2]  = {0, 0};
    int32_t hpRedState = 0;
    int32_t hpIrState  = 0;
    int32_t lpRedOut = 0;
    int32_t lpIrOut  = 0;
    int64_t sumRed = 0;
    int64_t sumIr  = 0;
    int bufferIdx = 0;

    PulseData *currentBuffer = NULL;

    if (xQueueReceive(emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
        Serial.println("Failed to get initial empty buffer");
        vTaskDelete(NULL);
        return;
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

        maxSensor.readNewData();

        while (maxSensor.available()) {
            MaxSample rawMaxData = maxSensor.readSample();

            if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
                medRed[0] = 0; medRed[1] = 0;
                medIr[0] = 0; medIr[1] = 0;
                hpRedState = 0;
                hpIrState  = 0;
                lpRedOut = 0;
                lpIrOut  = 0;
                sumRed = 0;
                sumIr  = 0;
                bufferIdx = 0;
                currentBuffer->dcRed = 0;
                currentBuffer->dcIr = 0;

                continue;
            }

            int32_t cleanRed = filters.medianFilter((int32_t)rawMaxData.Red, medRed);
            int32_t cleanIr = filters.medianFilter((int32_t)rawMaxData.Ir, medIr);

            medRed[1] = medRed[0]; medRed[0] = rawMaxData.Red;
            medIr[1] = medIr[0]; medIr[0] = rawMaxData.Ir;

            sumRed += cleanRed;
            sumIr += cleanIr;

            int32_t acRed = filters.highPassFilter(cleanRed, hpRedState);
            int32_t acIr = filters.highPassFilter(cleanIr, hpIrState);

            acRed = filters.lowPassFilter(acRed, lpRedOut);
            acIr = filters.lowPassFilter(acIr, lpIrOut);

            currentBuffer->acRed[bufferIdx] = acRed;
            currentBuffer->acIr[bufferIdx] = acIr;

            bufferIdx++;

            if (bufferIdx >= BUFFER_SIZE) {
                currentBuffer->dcRed = (int32_t)(sumRed / BUFFER_SIZE);
                currentBuffer->dcIr = (int32_t)(sumIr / BUFFER_SIZE);

                if (xQueueSend(fullQueue, &currentBuffer, portMAX_DELAY) != pdTRUE) {
                    Serial.println("Queue is full");
                    bufferIdx = 0;
                    sumRed = 0;
                    sumIr = 0;

                    continue;
                }

                if (xQueueReceive(emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
                    Serial.println("Failed to receive next empty buffer");
                    vTaskDelete(NULL);
                    return;
                }

                bufferIdx = 0;
                sumRed = 0;
                sumIr = 0; 
            }
        }
        // Serial.print("Minimalny wolny stos (High Water Mark): ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}

void vCalculateVitalsTask(void *pvParameters) {
    PulseData *processingBuffer = NULL;
    
    for (;;) {
        if (xQueueReceive(fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
            Serial.println("Failed to receive full buffer");
            continue;
        }

        int currentHR = 0;
        int32_t currentSpO2 = processor.calculateSpO2(*processingBuffer, SAMPLING_RATE, currentHR);

        if (currentHR > 0 && currentSpO2 > 0) {
            Serial.print("HR: ");
            Serial.print(currentHR);
            Serial.print(" BPM | SpO2: ");
            Serial.print(currentSpO2);
            Serial.println(" %");
        }

        if (xQueueSend(emptyQueue, &processingBuffer, portMAX_DELAY) != pdTRUE) {
            Serial.println("Failed to return buffer to emptyQueue");
        }
        // Serial.print("Minimalny wolny stos (High Water Mark): ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}