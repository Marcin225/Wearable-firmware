#include "tasks.h"
#include "sensor_processing.h"
#include "measurement_buffer.h"

TaskHandle_t CollectAndFilterTaskHandle = NULL;

void IRAM_ATTR max30102ISR() {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(CollectAndFilterTaskHandle, &higherPriorityTaskWoken);

    if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    
}

void resetMeasurementSession(SystemContext &sysCtx, int &buffer_idx, int &spo2_idx, bool &first_sample) {
    buffer_idx = 0;
    spo2_idx = 0;
    first_sample = true;
    // buffer_ready = false;
    // fill_stage = BufferWarmupStage::EMPTY;
    for (int s = 0; s < 4; s++) {
        sysCtx.algorithm.spo2Data.signal_sum_Ir[s] = 0;
        sysCtx.algorithm.spo2Data.signal_sum_Red[s] = 0;
    }

    sysCtx.measurementSessionId.fetch_add(1, std::memory_order_relaxed);
}

// collects ppg and accel data, applies basic filtering and builds synchronized stream

void vCollectAndFilterDataTask(void *pvParameters) {
    SystemContext *sysCtx = (SystemContext *)pvParameters;

    pulseData *currentBuffer = NULL;

    MpuSample mpuBatch[40] = {0};
    int maxCount = 0;
    int mpuCount = 0;

    if (xQueueReceive(sysCtx->emptyQueue, &currentBuffer, portMAX_DELAY) != pdTRUE || currentBuffer == NULL) {
        Serial.println("Failed to get initial empty buffer");
        vTaskDelete(NULL);
        return;
    }

    int buffer_idx = 0;
    int spo2_idx = 0;
    bool first_sample = true;

    bool is_finger_removed = false;
    TickType_t finger_removed_time = 0;
    int no_motion_counter = 0;
    int pulsation_no_signal_counter = 0;
    int pulsation_signal_counter = 0;
    int PULSATION_DELAY = 3000;

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

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sysCtx->maxSensor.readNewData();
        sysCtx->mpuSensor.readNewData();

        maxCount = sysCtx->maxSensor.available();
        mpuCount = 0;

        while (sysCtx->mpuSensor.available() && mpuCount < 40) {
            mpuBatch[mpuCount++] = sysCtx->mpuSensor.readSample();
        }

        switch (system_mode) {
                case DeviceState::WORK:

                    for (int s = 0; s < maxCount; s++) {
                        MaxSample rawMaxData = sysCtx->maxSensor.readSample();

                        if (rawMaxData.Ir < FINGER_IR_THRESHOLD) {
                            if (!is_finger_removed) {
                                finger_removed_time = xTaskGetTickCount();
                                is_finger_removed = true;

                                // RESET system
                                resetMeasurementSession(*sysCtx, buffer_idx, spo2_idx, first_sample);
                            }

                            if (xTaskGetTickCount() - finger_removed_time >= pdMS_TO_TICKS(15000)) {
                                // deep sleep esp and max30102 and set mpu6050 to check motion mode
                                is_finger_removed = false;

                                sysCtx->maxSensor.shutDown();
                                sysCtx->mpuSensor.enableWakeOnMotion();
                                esp_deep_sleep_start();
                            }

                            continue;
                        }

                        is_finger_removed = false;

                        MpuSample rawMpuData = interpolateMpu(mpuBatch, mpuCount, s, maxCount);
                        
                        currentBuffer->sample_buffer_Ir[buffer_idx] = processChannel(sysCtx->filter, ir, rawMaxData.Ir, first_sample);
                        currentBuffer->sample_buffer_Red[buffer_idx] = processChannel(sysCtx->filter, red, rawMaxData.Red, first_sample);

                        currentBuffer->sample_buffer_AccX[buffer_idx] = processChannel(sysCtx->filter, accX, (int32_t)rawMpuData.accX, first_sample);
                        currentBuffer->sample_buffer_AccY[buffer_idx] = processChannel(sysCtx->filter, accY, (int32_t)rawMpuData.accY, first_sample);
                        currentBuffer->sample_buffer_AccZ[buffer_idx] = processChannel(sysCtx->filter, accZ, (int32_t)rawMpuData.accZ, first_sample);

                        if (spo2_idx >= BUFFER_SIZE - CHUNK_SIZE) {
                            sysCtx->algorithm.spo2Data.signal_sum_Ir[3] += rawMaxData.Ir;
                            sysCtx->algorithm.spo2Data.signal_sum_Red[3] += rawMaxData.Red;
                            if (spo2_idx >= BUFFER_SIZE - 1) {

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

                                spo2_idx = BUFFER_SIZE - CHUNK_SIZE - 1;
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

                        currentBuffer->sessionId = sysCtx->measurementSessionId.load(std::memory_order_relaxed);

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

                    break;


                case DeviceState::CHECK:
                    
                    bool finger_found_in_batch = false;

                    for (int s = 0; s < maxCount; s++) {
                        MaxSample rawMaxData = sysCtx->maxSensor.readSample();
                        if (rawMaxData.Ir >= FINGER_IR_THRESHOLD) {
                            finger_found_in_batch = true;
                        }
                    }

                    if (finger_found_in_batch) {
                        pulsation_signal_counter++;
                        
                        if (pulsation_signal_counter >= 2) {
                            system_mode = DeviceState::WORK;

                            pulsation_no_signal_counter = 0;
                            pulsation_signal_counter = 0;
                            no_motion_counter = 0;
                            is_finger_removed = false;
                            PULSATION_DELAY = 3000;

                            // RESET system
                            resetMeasurementSession(*sysCtx, buffer_idx, spo2_idx, first_sample);
                        }

                    }else {
                        pulsation_signal_counter = 0;

                        if (mpuCount <= 0) {
                            no_motion_counter = 0;
                        } else {
                            int32_t motion = calculateMotion(mpuBatch, mpuCount);

                            if (motion < MOTION_THRESHOLD) {
                                no_motion_counter++;

                                if (no_motion_counter >= 3) {

                                    PULSATION_DELAY = 3000;
                                    sysCtx->maxSensor.shutDown();
                                    sysCtx->mpuSensor.enableWakeOnMotion();
                                    esp_deep_sleep_start();
                                }
                            } else {
                                pulsation_no_signal_counter++;

                                if (pulsation_no_signal_counter >= 5) {
                                    PULSATION_DELAY = 5000;
                                }

                                no_motion_counter = 0;
                            }
                        }

                        sysCtx->maxSensor.shutDown();
                        sysCtx->mpuSensor.sleep();

                        vTaskDelay(pdMS_TO_TICKS(PULSATION_DELAY));

                        sysCtx->maxSensor.wakeUp();
                        sysCtx->mpuSensor.wakeUp();
                    }

                    break;
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

    BufferWarmupStage fill_stage = BufferWarmupStage::EMPTY;
    uint32_t activeSessionId = UINT32_MAX;

    for (;;) {
        if (xQueueReceive(sysCtx->fullQueue, &processingBuffer, portMAX_DELAY) != pdTRUE || processingBuffer == NULL) {
            Serial.println("Failed to receive full buffer");
            continue;
        }

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
            Serial.print(sysCtx->algorithm.StateMachine.state);
            Serial.print("\n");
            Serial.print("\n");

            currentSessionId = sysCtx->measurementSessionId.load(std::memory_order_relaxed);

            if (activeSessionId != currentSessionId) {
                activeSessionId = currentSessionId;
                fill_stage = BufferWarmupStage::EMPTY;
                sysCtx->algorithm.reset_session();
            }
            
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