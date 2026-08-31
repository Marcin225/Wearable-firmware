#include "tasks.h"

#include "config.h"
#include "SystemContext.h"
#include "collector_task/collector_context.h"
#include "signal_channel.h"
#include "sensor_processing.h"
#include "measurement_buffer.h"
#include "spo2_dc.h"

TaskHandle_t CollectAndFilterTaskHandle = NULL;

// generate an active-low interrupt when the MAX30102 FIFO reaches the configured sample threshold (28/32)
// the ISR wakes the vCollectAndFilterDataTask to collect and process the available samples
void IRAM_ATTR max30102ISR() {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(CollectAndFilterTaskHandle, &higherPriorityTaskWoken);

    if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    
}

// collect and synchronize MAX30102 and MPU6050 data
// use the MAX30102 as the timing reference and interpolate MPU6050 samples
// apply initial median and Butterworth band-pass filtering
// manage device states: SLEEP, CHECK and WORK
void vCollectAndFilterDataTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;

    pulseData *currentBuffer = NULL;

    MpuSample mpuBatch[40] = {0};
    int maxCount = 0;
    int mpuCount = 0;

    // receive an empty buffer, fill it with processed samples and send it to the calculation task
    // two buffer queues are used to continuously exchange empty and full buffers
    if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
        Serial.println("Failed to get initial empty buffer");
        vTaskDelete(NULL);
        return;
    }

    CollectorState state;
    CollectorFilters filters;

    initCollectorFilters(sysCtx->filter, filters);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sysCtx->maxSensor.readNewData();
        sysCtx->mpuSensor.readNewData();

        maxCount = sysCtx->maxSensor.available();
        mpuCount = 0;

        while (sysCtx->mpuSensor.available() && mpuCount < 40) {
            mpuBatch[mpuCount++] = sysCtx->mpuSensor.readSample();
        }

        // collect synchronized PPG and motion data, apply filtering and update SpO2 DC components
        // enter deep sleep when no finger signal is detected for the configured timeout
        switch (sysCtx->systemMode) {
                case DeviceState::WORK: {

                    for (int s = 0; s < maxCount; s++) {
                        MaxSample rawMaxData = sysCtx->maxSensor.readSample();

                        if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
                            if (!state.is_finger_removed) {
                                state.finger_removed_time = xTaskGetTickCount();
                                state.is_finger_removed = true;

                                // RESET
                                resetMeasurementSession(*sysCtx, state);
                            }

                            if (xTaskGetTickCount() - state.finger_removed_time >= pdMS_TO_TICKS(15000)) {
                                state.is_finger_removed = false;

                                sysCtx->maxSensor.shutDown();
                                sysCtx->mpuSensor.enableWakeOnMotion();
                                esp_deep_sleep_start();
                            }

                            continue;
                        }

                        state.is_finger_removed = false;

                        MpuSample rawMpuData = interpolateMpu(mpuBatch, mpuCount, s, maxCount);
                        
                        currentBuffer->sample_buffer_Ir[state.buffer_idx] = processChannel(sysCtx->filter, filters.ir, rawMaxData.Ir, state.first_sample);
                        currentBuffer->sample_buffer_Red[state.buffer_idx] = processChannel(sysCtx->filter, filters.red, rawMaxData.Red, state.first_sample);

                        currentBuffer->sample_buffer_AccX[state.buffer_idx] = processChannel(sysCtx->filter, filters.accX, (int32_t)rawMpuData.accX, state.first_sample);
                        currentBuffer->sample_buffer_AccY[state.buffer_idx] = processChannel(sysCtx->filter, filters.accY, (int32_t)rawMpuData.accY, state.first_sample);
                        currentBuffer->sample_buffer_AccZ[state.buffer_idx] = processChannel(sysCtx->filter, filters.accZ, (int32_t)rawMpuData.accZ, state.first_sample);

                        updateSpo2Dc(sysCtx->algorithm.spo2Data, state.spo2_idx, rawMaxData.Ir, rawMaxData.Red);

                        state.buffer_idx++;
                        state.spo2_idx++;
                        state.first_sample = false;

                        if (state.buffer_idx < CHUNK_SIZE) {
                            continue;
                        }

                        currentBuffer->sessionId = sysCtx->measurementSessionId.load(std::memory_order_relaxed);

                        if (xQueueSend(sysCtx->fullQueue, &currentBuffer, portMAX_DELAY) != pdTRUE) {
                            Serial.println("Queue is full -> dropping packet");
                        }
                        if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
                            Serial.println("Failed to receive next empty buffer");
                            vTaskDelete(NULL);
                            return;
                        }

                        state.buffer_idx = 0;
                        
                    }

                    break;

                }

                // periodically check for a valid PPG signal and motion activity
                // return to WORK when a signal is detected or return to SLEEP when no motion is detected
                case DeviceState::CHECK: {
                    
                    bool finger_found_in_batch = false;

                    for (int s = 0; s < maxCount; s++) {
                        MaxSample rawMaxData = sysCtx->maxSensor.readSample();
                        if (rawMaxData.Ir >= FINGER_IR_THRESHOLD) {
                            finger_found_in_batch = true;
                        }
                    }

                    if (finger_found_in_batch) {
                        state.pulsation_signal_counter++;
                        
                        if (state.pulsation_signal_counter >= 2) {
                            sysCtx->systemMode = DeviceState::WORK;

                            state.pulsation_no_signal_counter = 0;
                            state.pulsation_signal_counter = 0;
                            state.no_motion_counter = 0;
                            state.is_finger_removed = false;
                            state.PULSATION_DELAY = 3000;

                            // RESET
                            resetMeasurementSession(*sysCtx, state);
                        }

                    }else {
                        state.pulsation_signal_counter = 0;

                        if (mpuCount <= 0) {
                            state.no_motion_counter = 0;
                        } else {
                            int32_t motion = calculateMotion(mpuBatch, mpuCount);

                            if (motion < MOTION_THRESHOLD) {
                                state.no_motion_counter++;

                                if (state.no_motion_counter >= 3) {

                                    state.PULSATION_DELAY = 3000;
                                    sysCtx->maxSensor.shutDown();
                                    sysCtx->mpuSensor.enableWakeOnMotion();
                                    esp_deep_sleep_start();
                                }
                            } else {
                                state.pulsation_no_signal_counter++;

                                if (state.pulsation_no_signal_counter >= 5) {
                                    state.PULSATION_DELAY = 5000;
                                }

                                state.no_motion_counter = 0;
                            }
                        }

                        sysCtx->maxSensor.shutDown();
                        sysCtx->mpuSensor.sleep();

                        vTaskDelay(pdMS_TO_TICKS(state.PULSATION_DELAY));

                        sysCtx->maxSensor.wakeUp();
                        sysCtx->mpuSensor.wakeUp();
                    }

                    break;
                }
        }

            // Stack Size

            // Serial.print("Free stack (High Water Mark): collector ");
            // Serial.println(uxTaskGetStackHighWaterMark(NULL));

    }
}


// process filled data chunks from the collector task
// combine four 256-sample chunks into a 1024-sample processing buffer
// calculate heart rate and SpO2 from the accumulated samples
void vCalculateVitalsTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;
    pulseData *processingBuffer = NULL;

    int32_t heartRate = 0;
    int32_t spo2 = 0;
    int32_t hrSmooth = 0;

    BufferWarmupStage fill_stage = BufferWarmupStage::EMPTY;
    uint32_t activeSessionId = UINT32_MAX;

    for (;;) {
        if (xQueueReceive(sysCtx->fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
            Serial.println("Failed to receive full buffer");
            continue;
        }

        // discard buffers from outdated measurement sessions to prevent stale data processing
        uint32_t currentSessionId = sysCtx->measurementSessionId.load(std::memory_order_relaxed);

        if (processingBuffer->sessionId != currentSessionId) {
            xQueueSend(sysCtx->emptyQueue, &processingBuffer, portMAX_DELAY);
            continue;
        }

        if (processingBuffer->sessionId != activeSessionId) {
            activeSessionId = processingBuffer->sessionId;
            fill_stage = BufferWarmupStage::EMPTY;
            sysCtx->algorithm.reset_session();
        }

        switch (fill_stage) {
            case BufferWarmupStage::READY:
                shiftProcessingBuffer(sysCtx->algorithm.processBuffer, BUFFER_SIZE - CHUNK_SIZE);

                copyChunkToProcessingBuffer(sysCtx->algorithm.processBuffer, *processingBuffer, BUFFER_SIZE - CHUNK_SIZE);

                break;
            
            case BufferWarmupStage::THREE_QUARTERS_FULL:
                copyChunkToProcessingBuffer(sysCtx->algorithm.processBuffer, *processingBuffer, BUFFER_SIZE - CHUNK_SIZE);
                fill_stage = BufferWarmupStage::READY;

                break;
            
            case BufferWarmupStage::HALF_FULL:
                copyChunkToProcessingBuffer(sysCtx->algorithm.processBuffer, *processingBuffer, CHUNK_SIZE * 2);
                fill_stage = BufferWarmupStage::THREE_QUARTERS_FULL;

                break;

            case BufferWarmupStage::QUARTER_FULL:
                copyChunkToProcessingBuffer(sysCtx->algorithm.processBuffer, *processingBuffer, CHUNK_SIZE);
                fill_stage = BufferWarmupStage::HALF_FULL;

                break;

            case BufferWarmupStage::EMPTY:
                copyChunkToProcessingBuffer(sysCtx->algorithm.processBuffer, *processingBuffer, 0);
                fill_stage = BufferWarmupStage::QUARTER_FULL;

                break;
        }

        if (fill_stage == BufferWarmupStage::READY) {

            VitalResult result = sysCtx->algorithm.calculateVitals(BONUS_Q12, MAIN_PENALTY_Q12, TH_CF_Q12);

            heartRate = result.heartRate;
            spo2 = result.spo2;
            hrSmooth = sysCtx->algorithm.smooth_hr(heartRate);
            
            Serial.print(hrSmooth);
            Serial.print(" ");
            Serial.print(spo2);
            Serial.print(" ");
            Serial.print(sysCtx->algorithm.getState());
            Serial.print("\n");
            Serial.print("\n");

            // // the collector task may invalidate the session while vitals are being calculated
            // // finish the current calculation, then reset the processing state if the session changed
            // currentSessionId = sysCtx->measurementSessionId.load(std::memory_order_relaxed);

            // if (activeSessionId != currentSessionId) {
            //     activeSessionId = currentSessionId;
            //     fill_stage = BufferWarmupStage::EMPTY;
            //     sysCtx->algorithm.reset_session();
            // }
            
        }

        if (xQueueSend(sysCtx->emptyQueue, &processingBuffer, portMAX_DELAY) != pdTRUE) {
            Serial.println("Failed to return buffer to emptyQueue");
        }

        // if (sysCtx->BLE.getConnectionState()) {
        //     sysCtx->BLE.sendPackage((uint8_t)currentHR, (uint8_t)currentSpO2, 99);
        // }

        // Stack Size

        // Serial.print("Free stack (High Water Mark): calculation");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
}