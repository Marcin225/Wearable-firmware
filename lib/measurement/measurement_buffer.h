#ifndef MEASUREMENT_BUFFER_H
#define MEASUREMENT_BUFFER_H

#include <stddef.h>
#include "measurement_types.h"

enum class BufferWarmupStage {
    EMPTY = 0,
    QUARTER_FULL = 1,
    HALF_FULL = 2,
    THREE_QUARTERS_FULL = 3,
    READY = 4
};

void copyChunkToProcessingBuffer(estimationBuffer &destination, const pulseData &source, size_t offset);
void shiftProcessingBuffer(estimationBuffer &buffer, size_t data_size);

#endif