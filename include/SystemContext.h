#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

// central state structure passed to tasks to improve readability and prevent code duplication

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "max30102_driver.h"
#include "mpu6050_driver.h"
#include "PpgProcessor.h"
#include "algorithm_NLMS.h"
#include "BLE.h"
#include "config.h"

struct SystemContext {
    MAX30102 maxSensor;
    MPU6050 mpuSensor;
    FilterAlgorithms filters;
    SignalProcessingAlgorithms processor;

    BleManager BLE;

    PulseData pulseBufferA;
    PulseData pulseBufferB;

    QueueHandle_t emptyQueue = NULL;
    QueueHandle_t fullQueue = NULL;
};

#endif