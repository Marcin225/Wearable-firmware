#include "helpers.h"

MAX30102 maxSensor;
MPU6050 mpuSensor;
FilterAlgorithms filters;
SignalProcessingAlgorithms processor;

PulseData pulseBufferA, pulseBufferB;

QueueHandle_t emptyQueue = NULL;
QueueHandle_t fullQueue = NULL;