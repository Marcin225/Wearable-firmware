// #include "tasks.h"

// // collects ppg and accel data, applies basic filtering and builds synchronized stream

// void vCollectAndFilterDataTask(void *pvParameters) {
//     SystemContext *sysCtx = (SystemContext *)pvParameters;

//     OpticalChannel redChannel;
//     OpticalChannel irChannel;
//     AxisFilter accX, accY, accZ;

//     int32_t lpMotionOut = 0;
//     int32_t lpMotionDc = 0;
//     int bufferIdx = 0;

//     PulseData *currentBuffer = NULL;

//     MpuSample lastMpuData = {0, 0, 0, 0, 0, 0};

//     if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
//         Serial.println("Failed to get initial empty buffer");
//         vTaskDelete(NULL);
//         return;
//     }

//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     for (;;) {
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

//         sysCtx->maxSensor.readNewData();
//         sysCtx->mpuSensor.readNewData();

//         while (sysCtx->maxSensor.available()) {
//             MaxSample rawMaxData = sysCtx->maxSensor.readSample();
//             // MpuSample rawMpuData = {0,0,0,0,0,0};

//             while (sysCtx->mpuSensor.available()) {
//                 lastMpuData = sysCtx->mpuSensor.readSample();
//                 // rawMpuData = sysCtx->mpuSensor.readSample();
//             }

//             MpuSample rawMpuData = lastMpuData;
            
//             // finger detection: drop data and reset signal filters if the sensor is uncovered
//             if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
//                 redChannel.reset();
//                 irChannel.reset();
//                 accX.reset();
//                 accY.reset();
//                 accZ.reset();
//                 lpMotionOut = 0;
//                 lpMotionDc  = 0;

//                 bufferIdx = 0;
//                 currentBuffer->dcRed = 0;
//                 currentBuffer->dcIr = 0;

//                 continue;
//             }

//             // DSP pipeline: remove DC offset -> apply low-pass filtering -> sync motion data with heart rate signal
//             currentBuffer->acRed[bufferIdx] = redChannel.process((int32_t)rawMaxData.Red, sysCtx->filters);
//             currentBuffer->acIr[bufferIdx] = irChannel.process((int32_t)rawMaxData.Ir, sysCtx->filters);

//             int32_t cleanAccelx = accX.process(rawMpuData.accX, sysCtx->filters);
//             int32_t cleanAccely = accY.process(rawMpuData.accY, sysCtx->filters);
//             int32_t cleanAccelz = accZ.process(rawMpuData.accZ, sysCtx->filters);

//             int32_t motion = sysCtx->filters.absValueOf(cleanAccelx) + sysCtx->filters.absValueOf(cleanAccely) 
//                             + sysCtx->filters.absValueOf(cleanAccelz);

//             motion = sysCtx->filters.lowPassFilter(motion, lpMotionOut);

//             int32_t finalMotion = motion - sysCtx->filters.lowPassFilter(motion, lpMotionDc, 4);

//             currentBuffer->motionNoise[bufferIdx] = finalMotion;

//             bufferIdx++;

//             if (bufferIdx >= BUFFER_SIZE) {
//                 currentBuffer->dcRed = (int32_t)(redChannel.getSum() / BUFFER_SIZE);
//                 currentBuffer->dcIr = (int32_t)(irChannel.getSum() / BUFFER_SIZE);

//                 // Serial.println(finalMotion);

//                 if (xQueueSend(sysCtx->fullQueue, &currentBuffer, portMAX_DELAY) != pdTRUE) {
//                     Serial.println("Queue is full -> dropping packet");
//                 }

//                 if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
//                     Serial.println("Failed to receive next empty buffer");
//                     vTaskDelete(NULL);
//                     return;
//                 }

//                 bufferIdx = 0;
//                 redChannel.resetSum();
//                 irChannel.resetSum();
//             }
//         }
//         // Stack Size

//         // Serial.print("Free stack (High Water Mark): ");
//         // Serial.println(uxTaskGetStackHighWaterMark(NULL));
//     }
// }

// // processes buffered PPG data and estimates vital signs (HR, SpO2)

// void vCalculateVitalsTask(void *pvParameters) {
//     SystemContext *sysCtx = (SystemContext *)pvParameters;
//     PulseData *processingBuffer = NULL;

//     static int32_t filterWeightsIr[NLMS_NUM_OF_TAPS] = {0};
//     static int32_t filterWeightsRed[NLMS_NUM_OF_TAPS] = {0};
//     static int32_t noiseHistoryIr[NLMS_NUM_OF_TAPS] = {0};
//     static int32_t noiseHistoryRed[NLMS_NUM_OF_TAPS] = {0};

//     for (;;) {
//         if (xQueueReceive(sysCtx->fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
//             Serial.println("Failed to receive full buffer");
//             continue;
//         }
//         // NLMS filter

//         // NLMS(processingBuffer->motionNoise, filterWeightsIr, processingBuffer->acIr, 
//         //     512, 1, NLMS_NUM_OF_TAPS, BUFFER_SIZE, noiseHistoryIr);
//         // NLMS(processingBuffer->motionNoise, filterWeightsRed, processingBuffer->acRed, 
//         //     512, 1, NLMS_NUM_OF_TAPS, BUFFER_SIZE, noiseHistoryRed);

//         int currentHR = 0;
//         int32_t currentSpO2 = sysCtx->processor.calculateSpO2(*processingBuffer, SAMPLING_RATE_HZ, currentHR);

//         if (currentHR > 0 && currentSpO2 > 0) {
//             Serial.print("HR: ");
//             Serial.print(currentHR);
//             Serial.print(" BPM | SpO2: ");
//             Serial.print(currentSpO2);
//             Serial.println(" %");
//         }else {
//             Serial.println("Calculating...");
//         }

//         if (xQueueSend(sysCtx->emptyQueue, &processingBuffer, portMAX_DELAY) != pdTRUE) {
//             Serial.println("Failed to return buffer to emptyQueue");
//         }

//         // if (sysCtx->BLE.getConnectionState()) {
//         //     sysCtx->BLE.sendPackage((uint8_t)currentHR, (uint8_t)currentSpO2, 99);
//         // }

//         // Stack Size

//         // Serial.print("Free stack (High Water Mark): ");
//         // Serial.println(uxTaskGetStackHighWaterMark(NULL));
//     }
// }


#include "tasks.h"

// collects ppg and accel data, applies basic filtering and builds synchronized stream

void vCollectAndFilterDataTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;

    pulseData *currentBuffer = NULL;

    MpuSample lastMpuData = {0, 0, 0, 0, 0, 0};

    if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
        Serial.println("Failed to get initial empty buffer");
        vTaskDelete(NULL);
        return;
    }

    int buffer_idx = 0;
    int spo2_idx = 0;
    bool first_sample = true;

    ChannelFilter ir;
    ChannelFilter red;
    ChannelFilter accX;
    ChannelFilter accY;
    ChannelFilter accZ;

    initChannel(sysCtx->filter, ir);
    initChannel(sysCtx->filter, red);
    initChannel(sysCtx->filter, accX);
    initChannel(sysCtx->filter, accY);
    initChannel(sysCtx->filter, accZ);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

        sysCtx->maxSensor.readNewData();
        sysCtx->mpuSensor.readNewData();

        while (sysCtx->maxSensor.available()) {
            MaxSample rawMaxData = sysCtx->maxSensor.readSample();

            while (sysCtx->mpuSensor.available()) {
                lastMpuData = sysCtx->mpuSensor.readSample();
            }

            MpuSample rawMpuData = lastMpuData;
            
            // finger detection: drop data and reset signal filters if the sensor is uncovered
            if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
                buffer_idx = 0;
                spo2_idx = 0;
                first_sample = true;
                buffer_ready = false;
                fill_stage = BufferWarmupStage::EMPTY;

                continue;
            }

            // DSP pipeline: remove DC offset -> apply low-pass filtering -> sync motion data with heart rate signal
            currentBuffer->sample_buffer_Ir[buffer_idx] = processChannel(sysCtx->filter, ir, rawMaxData.Ir, first_sample);
            currentBuffer->sample_buffer_Red[buffer_idx] = processChannel(sysCtx->filter, red, rawMaxData.Red, first_sample);

            currentBuffer->sample_buffer_AccX[buffer_idx] = processChannel(sysCtx->filter, accX, (int32_t)rawMpuData.accX, first_sample);
            currentBuffer->sample_buffer_AccY[buffer_idx] = processChannel(sysCtx->filter, accY, (int32_t)rawMpuData.accY, first_sample);
            currentBuffer->sample_buffer_AccZ[buffer_idx] = processChannel(sysCtx->filter, accZ, (int32_t)rawMpuData.accZ, first_sample);

            if (spo2_idx >= BUFFER_SIZE - CHUNK_SIZE) {
                sysCtx->algorithm.spo2Data.signal_sum_Ir[3] += rawMaxData.Ir;
                sysCtx->algorithm.spo2Data.signal_sum_Red[3] += rawMaxData.Red;
                if (spo2_idx >= BUFFER_SIZE) {

                    int64_t sumIr = 0;
                    int64_t sumRed = 0;
                    for (int s = 0; s < 4; s++) {
                        sumIr += sysCtx->algorithm.spo2Data.signal_sum_Ir[s];
                        sumRed += sysCtx->algorithm.spo2Data.signal_sum_Red[s];
                    }

                    sysCtx->algorithm.spo2Data.dcIr = sumIr / BUFFER_SIZE;
                    sysCtx->algorithm.spo2Data.dcRed = sumRed / BUFFER_SIZE;

                    shiftDcSignalSum(sysCtx->algorithm.spo2Data.signal_sum_Ir);
                    shiftDcSignalSum(sysCtx->algorithm.spo2Data.signal_sum_Red);

                    spo2_idx = BUFFER_SIZE - CHUNK_SIZE;
                }
            }else if (spo2_idx >= 2 * CHUNK_SIZE && spo2_idx < BUFFER_SIZE - CHUNK_SIZE) {
                sysCtx->algorithm.spo2Data.signal_sum_Ir[2] += rawMaxData.Ir;
                sysCtx->algorithm.spo2Data.signal_sum_Red[2] += rawMaxData.Red;
            }else if (spo2_idx >= CHUNK_SIZE && spo2_idx <  2 * CHUNK_SIZE) {
                sysCtx->algorithm.spo2Data.signal_sum_Ir[1] += rawMaxData.Ir;
                sysCtx->algorithm.spo2Data.signal_sum_Red[1] += rawMaxData.Red;
            }else if (spo2_idx >= 0 && spo2_idx < CHUNK_SIZE) {
                sysCtx->algorithm.spo2Data.signal_sum_Ir[0] += rawMaxData.Ir;
                sysCtx->algorithm.spo2Data.signal_sum_Red[0] += rawMaxData.Red;
            }

            buffer_idx++;
            spo2_idx++;
            first_sample = false;

            if (buffer_idx < CHUNK_SIZE) {
                continue;
            }

            if (xQueueSend(sysCtx->fullQueue, &currentBuffer, portMAX_DELAY) != pdTRUE) {
                Serial.println("Queue is full -> dropping packet");
            }

            if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
                Serial.println("Failed to receive next empty buffer");
                vTaskDelete(NULL);
                return;
            }

            buffer_idx = 0;
        }
        // Stack Size

        // Serial.print("Free stack (High Water Mark): ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}

// processes buffered PPG data and estimates vital signs (HR, SpO2)

void vCalculateVitalsTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;
    pulseData *processingBuffer = NULL;

    int32_t heartRate = 0;
    int32_t spo2 = 0;
    int32_t hrSmooth = 0;

    for (;;) {
        if (xQueueReceive(sysCtx->fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
            Serial.println("Failed to receive full buffer");
            continue;
        }

       
        switch (fill_stage) {
            case BufferWarmupStage::READY:
                memmove(sysCtx->algorithm.processBuffer.sample_buffer_Ir, 
                    sysCtx->algorithm.processBuffer.sample_buffer_Ir + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
                memmove(sysCtx->algorithm.processBuffer.sample_buffer_Red, 
                    sysCtx->algorithm.processBuffer.sample_buffer_Red + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
                memmove(sysCtx->algorithm.processBuffer.sample_buffer_AccX, 
                    sysCtx->algorithm.processBuffer.sample_buffer_AccX + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
                memmove(sysCtx->algorithm.processBuffer.sample_buffer_AccY, 
                    sysCtx->algorithm.processBuffer.sample_buffer_AccY + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
                memmove(sysCtx->algorithm.processBuffer.sample_buffer_AccZ, 
                    sysCtx->algorithm.processBuffer.sample_buffer_AccZ + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Ir + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Ir, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Red + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Red, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccX + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccX, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccY + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccY, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccZ + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccZ, CHUNK_SIZE * sizeof(int32_t));

                break;
            
            case BufferWarmupStage::THREE_QUARTERS_FULL:
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Ir + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Ir, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Red + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Red, CHUNK_SIZE * sizeof(int32_t));

                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccX + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccX, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccY + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccY, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccZ + BUFFER_SIZE - CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccZ, CHUNK_SIZE * sizeof(int32_t));
                fill_stage = BufferWarmupStage::READY;

                buffer_ready = true;

                break;
            
            case BufferWarmupStage::HALF_FULL:
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Ir + 2 * CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Ir, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Red + 2 * CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Red, CHUNK_SIZE * sizeof(int32_t));

                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccX + 2 * CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccX, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccY + 2 * CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccY, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccZ + 2 * CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccZ, CHUNK_SIZE * sizeof(int32_t));
                fill_stage = BufferWarmupStage::THREE_QUARTERS_FULL;

                break;

            case BufferWarmupStage::QUARTER_FULL:
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Ir + CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Ir, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Red + CHUNK_SIZE, 
                    processingBuffer->sample_buffer_Red, CHUNK_SIZE * sizeof(int32_t));

                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccX + CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccX, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccY + CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccY, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccZ + CHUNK_SIZE, 
                    processingBuffer->sample_buffer_AccZ, CHUNK_SIZE * sizeof(int32_t));
                fill_stage = BufferWarmupStage::HALF_FULL;

                break;

            case BufferWarmupStage::EMPTY:
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Ir, 
                    processingBuffer->sample_buffer_Ir, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_Red, 
                    processingBuffer->sample_buffer_Red, CHUNK_SIZE * sizeof(int32_t));

                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccX, 
                    processingBuffer->sample_buffer_AccX, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccY, 
                    processingBuffer->sample_buffer_AccY, CHUNK_SIZE * sizeof(int32_t));
                memcpy(sysCtx->algorithm.processBuffer.sample_buffer_AccZ, 
                    processingBuffer->sample_buffer_AccZ, CHUNK_SIZE * sizeof(int32_t));
                fill_stage = BufferWarmupStage::QUARTER_FULL;

                break;
        }

        if (buffer_ready) {

            sysCtx->algorithm.process_rfft(sysCtx->algorithm.processBuffer.sample_buffer_Ir, 
                sysCtx->algorithm.sharedFftBuffer.re_1, sysCtx->algorithm.sharedFftBuffer.im_1, FFT_SIZE);
            sysCtx->algorithm.calculate_hr_candidates(sysCtx->algorithm.sharedFftBuffer.re_1, sysCtx->algorithm.sharedFftBuffer.im_1);

            sysCtx->algorithm.process_rfft(sysCtx->algorithm.processBuffer.sample_buffer_AccX, 
                sysCtx->algorithm.sharedFftBuffer.re_1, sysCtx->algorithm.sharedFftBuffer.im_1, FFT_SIZE);
            sysCtx->algorithm.process_rfft(sysCtx->algorithm.processBuffer.sample_buffer_AccY, 
                sysCtx->algorithm.sharedFftBuffer.re_2, sysCtx->algorithm.sharedFftBuffer.im_2, FFT_SIZE);
            sysCtx->algorithm.process_rfft(sysCtx->algorithm.processBuffer.sample_buffer_AccZ, 
                sysCtx->algorithm.sharedFftBuffer.re_3, sysCtx->algorithm.sharedFftBuffer.im_3, FFT_SIZE);

            sysCtx->algorithm.calculate_motion_frequencies(sysCtx->algorithm.sharedFftBuffer.re_1, sysCtx->algorithm.sharedFftBuffer.im_1,
                sysCtx->algorithm.sharedFftBuffer.re_2, sysCtx->algorithm.sharedFftBuffer.im_2,
                sysCtx->algorithm.sharedFftBuffer.re_3, sysCtx->algorithm.sharedFftBuffer.im_3);

            heartRate = sysCtx->algorithm.calculate_hr(BONUS_Q12, MAIN_PENALTY_Q12, TH_CF_Q12);

            sysCtx->algorithm.process_single_bin_fft(sysCtx->algorithm.processBuffer.sample_buffer_Red,
                sysCtx->algorithm.sharedFftBuffer.re_1, sysCtx->algorithm.spo2Data.bin, FFT_SIZE);

            if (heartRate > 0 && sysCtx->algorithm.StateMachine.state == 0) {
                spo2 = sysCtx->algorithm.calculate_spo2();

            }else {
                spo2 = 0;
            }   

            hrSmooth = sysCtx->algorithm.smooth_hr(heartRate);
            Serial.print(hrSmooth);
            Serial.print(" ");
            Serial.print(spo2);
            Serial.print(" ");
            Serial.print(sysCtx->algorithm.StateMachine.state);
            Serial.print("\n");
            
        }

        if (xQueueSend(sysCtx->emptyQueue, &processingBuffer, portMAX_DELAY) != pdTRUE) {
            Serial.println("Failed to return buffer to emptyQueue");
        }

        // if (sysCtx->BLE.getConnectionState()) {
        //     sysCtx->BLE.sendPackage((uint8_t)currentHR, (uint8_t)currentSpO2, 99);
        // }

        // Stack Size

        // Serial.print("Free stack (High Water Mark): ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}