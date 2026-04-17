#ifndef HELPERS_H
#define HELPERS_H

#include "max30102_driver.h"
#include "mpu6050_driver.h"
#include "algorithm_NLMS.h"
#include "algorithm_HR.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define SAMPLING_RATE           100
#define FINGER_IR_THRESHOLD     50000   

extern MAX30102 maxSensor;
extern MPU6050 mpuSensor;

extern FilterAlgorithms filters;
extern SignalProcessingAlgorithms processor;
extern PulseData pulseBufferA, pulseBufferB;

extern QueueHandle_t emptyQueue;
extern QueueHandle_t fullQueue;

#endif