#include "tasks.h"

// collects ppg and accel data, applies basic filtering and builds synchronized stream

void vCollectAndFilterDataTask(void *pvParameters) {
    int32_t medRed[2] = {0, 0};
    int32_t medIr[2]  = {0, 0};
    int32_t hpRedState = 0;
    int32_t hpIrState  = 0;
    int32_t lpRedOut = 0;
    int32_t lpIrOut  = 0;
    int64_t sumRed = 0;
    int64_t sumIr  = 0;

    int32_t lpAccXOut = 0;
    int32_t lpAccYOut = 0;
    int32_t lpAccZOut = 0;
    int32_t lpMotionOut = 0;
    int32_t lpMotionDc = 0;
    MpuSample rawMpuData = {0,0,0,0,0,0};

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
        mpuSensor.readNewData();

        while (maxSensor.available()) {
            MaxSample rawMaxData = maxSensor.readSample();

            while (mpuSensor.available()) {
                rawMpuData = mpuSensor.readSample();
            }

            if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
                medRed[0] = 0; medRed[1] = 0;
                medIr[0] = 0; medIr[1] = 0;
                hpRedState = 0;
                hpIrState  = 0;
                lpRedOut = 0;
                lpIrOut  = 0;
                sumRed = 0;
                sumIr  = 0;
                lpAccXOut   = 0;
                lpAccYOut   = 0;
                lpAccZOut   = 0;
                lpMotionOut = 0;
                lpMotionDc  = 0;
                rawMpuData = {0,0,0,0,0,0};
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

            int32_t cleanAccelx = rawMpuData.accX - filters.lowPassFilter(rawMpuData.accX, lpAccXOut);
            int32_t cleanAccely = rawMpuData.accY - filters.lowPassFilter(rawMpuData.accY, lpAccYOut);
            int32_t cleanAccelz = rawMpuData.accZ - filters.lowPassFilter(rawMpuData.accZ, lpAccZOut);

            int32_t motion = filters.absValueOf(cleanAccelx) + filters.absValueOf(cleanAccely) 
                            + filters.absValueOf(cleanAccelz);

            motion = filters.lowPassFilter(motion, lpMotionOut);

            int32_t finalMotion = motion - filters.lowPassFilter(motion, lpMotionDc, 4);

            currentBuffer->motionNoise[bufferIdx] = finalMotion;

            bufferIdx++;

            if (bufferIdx >= BUFFER_SIZE) {
                currentBuffer->dcRed = (int32_t)(sumRed / BUFFER_SIZE);
                currentBuffer->dcIr = (int32_t)(sumIr / BUFFER_SIZE);

                // Serial.println(finalMotion);

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

                // Serial.print("Free stack (High Water Mark): ");
                // Serial.println(uxTaskGetStackHighWaterMark(NULL));
            }
        }
    }
}

// processes buffered PPG data and estimates vital signs (HR, SpO2)

void vCalculateVitalsTask(void *pvParameters) {
    PulseData *processingBuffer = NULL;
    
    #define NLMS_NUM_OF_TAPS                            32

    static int32_t filterWeightsIr[NLMS_NUM_OF_TAPS] = {0};
    static int32_t filterWeightsRed[NLMS_NUM_OF_TAPS] = {0};

    // static int32_t acIrBefore[BUFFER_SIZE];
    // static int32_t acRedBefore[BUFFER_SIZE];

    // static int32_t noiseHistoryIr[NLMS_NUM_OF_TAPS] = {0};
    // static int32_t noiseHistoryRed[NLMS_NUM_OF_TAPS] = {0};

    for (;;) {
        if (xQueueReceive(fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
            Serial.println("Failed to receive full buffer");
            continue;
        }

        // for (int i = 0; i < BUFFER_SIZE; i++) {
        //     acIrBefore[i] = processingBuffer->acIr[i];
        //     acRedBefore[i] = processingBuffer->acRed[i];
        // }
        
        // NLMS(processingBuffer->motionNoise, filterWeightsIr, processingBuffer->acIr, 
        //     512, 1, NLMS_NUM_OF_TAPS, BUFFER_SIZE, noiseHistoryIr);
        // NLMS(processingBuffer->motionNoise, filterWeightsRed, processingBuffer->acRed, 
        //     512, 1, NLMS_NUM_OF_TAPS, BUFFER_SIZE, noiseHistoryRed);

        int currentHR = 0;
        int32_t currentSpO2 = processor.calculateSpO2(*processingBuffer, SAMPLING_RATE, currentHR);

        // for (int i = 0; i < BUFFER_SIZE; i++) {
        //     Serial.print(acIrBefore[i]);
        //     Serial.print(',');
        //     Serial.print(processingBuffer->acIr[i]);
        //     Serial.print(',');
        //     Serial.print(acRedBefore[i]);
        //     Serial.print(',');
        //     Serial.print(processingBuffer->acRed[i]);
        //     Serial.print(',');
        //     Serial.println(processingBuffer->motionNoise[i]);
        // }

        if (currentHR > 0 && currentSpO2 > 0) {
            Serial.print("HR: ");
            Serial.print(currentHR);
            Serial.print(" BPM | SpO2: ");
            Serial.print(currentSpO2);
            Serial.println(" %");
        }else {
            Serial.println("Calculating...");
        }

        if (xQueueSend(emptyQueue, &processingBuffer, portMAX_DELAY) != pdTRUE) {
            Serial.println("Failed to return buffer to emptyQueue");
        }
        // Serial.print("Free stack (High Water Mark): ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}