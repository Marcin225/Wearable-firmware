#ifndef MEASUREMENT_BUFFER_H
#define MEASUREMENT_BUFFER_H

#include <stddef.h>
#include "post_processor.h"

void copyChunkToProcessingBuffer(estimationBuffer &destination, const pulseData &source, size_t offset);

void shiftProcessingBuffer(estimationBuffer &buffer, size_t data_size);

#endif