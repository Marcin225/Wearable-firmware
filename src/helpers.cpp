#include "helpers.h"

MAX30102 maxSensor;
FilterAlgorithms filters;
SignalProcessingAlgorithms processor;

PulseData pulseBufferA, pulseBufferB;

QueueHandle_t emptyQueue = NULL;
QueueHandle_t fullQueue = NULL;