#ifndef MEASUREMENT_TYPES_H
#define MEASUREMENT_TYPES_H

#include <stdint.h>
#include "../../include/config.h"

// stores one filtered measurement chunk passed between the collector and calculation tasks
struct pulseData {
    uint32_t sessionId = 0;

    int32_t sample_buffer_Ir[CHUNK_SIZE];
    int32_t sample_buffer_Red[CHUNK_SIZE];
    int32_t sample_buffer_AccX[CHUNK_SIZE];
    int32_t sample_buffer_AccY[CHUNK_SIZE];
    int32_t sample_buffer_AccZ[CHUNK_SIZE];
};

// stores the full measurement window used for HR and SpO2 calculations
struct estimationBuffer {
    int32_t sample_buffer_Ir[BUFFER_SIZE];
    int32_t sample_buffer_Red[BUFFER_SIZE];
    int32_t sample_buffer_AccX[BUFFER_SIZE];
    int32_t sample_buffer_AccY[BUFFER_SIZE];
    int32_t sample_buffer_AccZ[BUFFER_SIZE];
};

#endif