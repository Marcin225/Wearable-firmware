#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "max30102_driver.h"
#include "mpu6050_driver.h"
#include "PpgProcessor.h"
#include "algorithm_NLMS.h"
#include "config.h"

struct SystemContext {
    MAX30102 maxSensor;
    MPU6050 mpuSensor;
    FilterAlgorithms filters;
    SignalProcessingAlgorithms processor;

    PulseData pulseBufferA;
    PulseData pulseBufferB;

    QueueHandle_t emptyQueue = NULL;
    QueueHandle_t fullQueue = NULL;
};

#endif