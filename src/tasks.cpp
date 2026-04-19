#include "tasks.h"

// collects ppg and accel data, applies basic filtering and builds synchronized stream

void vCollectAndFilterDataTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;

    OpticalChannel redChannel;
    OpticalChannel irChannel;
    AxisFilter accX, accY, accZ;

    int32_t lpMotionOut = 0;
    int32_t lpMotionDc = 0;
    int bufferIdx = 0;

    PulseData *currentBuffer = NULL;

    if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
        Serial.println("Failed to get initial empty buffer");
        vTaskDelete(NULL);
        return;
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

        sysCtx->maxSensor.readNewData();
        sysCtx->mpuSensor.readNewData();

        while (sysCtx->maxSensor.available()) {
            MaxSample rawMaxData = sysCtx->maxSensor.readSample();
            MpuSample rawMpuData = {0,0,0,0,0,0};

            while (sysCtx->mpuSensor.available()) {
                rawMpuData = sysCtx->mpuSensor.readSample();
            }

            if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
                redChannel.reset();
                irChannel.reset();
                accX.reset();
                accY.reset();
                accZ.reset();
                lpMotionOut = 0;
                lpMotionDc  = 0;

                bufferIdx = 0;
                currentBuffer->dcRed = 0;
                currentBuffer->dcIr = 0;

                continue;
            }

            currentBuffer->acRed[bufferIdx] = redChannel.process((int32_t)rawMaxData.Red, sysCtx->filters);
            currentBuffer->acIr[bufferIdx] = irChannel.process((int32_t)rawMaxData.Ir, sysCtx->filters);

            int32_t cleanAccelx = accX.process(rawMpuData.accX, sysCtx->filters);
            int32_t cleanAccely = accY.process(rawMpuData.accY, sysCtx->filters);
            int32_t cleanAccelz = accZ.process(rawMpuData.accZ, sysCtx->filters);

            int32_t motion = sysCtx->filters.absValueOf(cleanAccelx) + sysCtx->filters.absValueOf(cleanAccely) 
                            + sysCtx->filters.absValueOf(cleanAccelz);

            motion = sysCtx->filters.lowPassFilter(motion, lpMotionOut);

            int32_t finalMotion = motion - sysCtx->filters.lowPassFilter(motion, lpMotionDc, 4);

            currentBuffer->motionNoise[bufferIdx] = finalMotion;

            bufferIdx++;

            if (bufferIdx >= BUFFER_SIZE) {
                currentBuffer->dcRed = (int32_t)(redChannel.getSum() / BUFFER_SIZE);
                currentBuffer->dcIr = (int32_t)(irChannel.getSum() / BUFFER_SIZE);

                // Serial.println(finalMotion);

                if (xQueueSend(sysCtx->fullQueue, &currentBuffer, portMAX_DELAY) != pdTRUE) {
                    Serial.println("Queue is full -> dropping packet");
                }

                if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
                    Serial.println("Failed to receive next empty buffer");
                    vTaskDelete(NULL);
                    return;
                }

                bufferIdx = 0;
                redChannel.resetSum();
                irChannel.resetSum();

                // Serial.print("Free stack (High Water Mark): ");
                // Serial.println(uxTaskGetStackHighWaterMark(NULL));
            }
        }
    }
}

// processes buffered PPG data and estimates vital signs (HR, SpO2)

void vCalculateVitalsTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;
    PulseData *processingBuffer = NULL;

    static int32_t filterWeightsIr[NLMS_NUM_OF_TAPS] = {0};
    static int32_t filterWeightsRed[NLMS_NUM_OF_TAPS] = {0};
    static int32_t noiseHistoryIr[NLMS_NUM_OF_TAPS] = {0};
    static int32_t noiseHistoryRed[NLMS_NUM_OF_TAPS] = {0};

    // static int32_t acIrBefore[BUFFER_SIZE];
    // static int32_t acRedBefore[BUFFER_SIZE];

    for (;;) {
        if (xQueueReceive(sysCtx->fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
            Serial.println("Failed to receive full buffer");
            continue;
        }

        // Debugging

        // for (int i = 0; i < BUFFER_SIZE; i++) {
        //     acIrBefore[i] = processingBuffer->acIr[i];
        //     acRedBefore[i] = processingBuffer->acRed[i];
        // }
        
        // NLMS(processingBuffer->motionNoise, filterWeightsIr, processingBuffer->acIr, 
        //     512, 1, NLMS_NUM_OF_TAPS, BUFFER_SIZE, noiseHistoryIr);
        // NLMS(processingBuffer->motionNoise, filterWeightsRed, processingBuffer->acRed, 
        //     512, 1, NLMS_NUM_OF_TAPS, BUFFER_SIZE, noiseHistoryRed);

        int currentHR = 0;
        int32_t currentSpO2 = sysCtx->processor.calculateSpO2(*processingBuffer, SAMPLING_RATE_HZ, currentHR);

        // Debugging

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

        if (xQueueSend(sysCtx->emptyQueue, &processingBuffer, portMAX_DELAY) != pdTRUE) {
            Serial.println("Failed to return buffer to emptyQueue");
        }

        // Stack Size

        // Serial.print("Free stack (High Water Mark): ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}