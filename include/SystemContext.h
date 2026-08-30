#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

// shared system state passed to FreeRTOS tasks

#include <stdint.h>
#include <atomic>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "max30102_driver.h"
#include "mpu6050_driver.h"

#include "pre_processor.h"
#include "post_processor.h"
#include "measurement_types.h"

#include "BLE.h"

// current device operating mode
enum class DeviceState {
    CHECK = 0,
    WORK = 1
};

struct SystemContext {
    MAX30102 maxSensor;
    MPU6050 mpuSensor;

    FilterAlgorithms filter;
    SignalProcessingAlgorithms algorithm;

    BleManager BLE;

    pulseData pulseBufferA;
    pulseData pulseBufferB;

    // queues used to pass measurement buffers between task
    QueueHandle_t emptyQueue = NULL;
    QueueHandle_t fullQueue = NULL;

    // identifies the current continuous measurement session
    std::atomic<uint32_t> measurementSessionId{0};

    DeviceState systemMode = DeviceState::WORK;
};

#endif