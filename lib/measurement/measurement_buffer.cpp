#include "measurement_buffer.h"

#include <cstring>
#include "../../include/config.h"

// copy a data chunk into the processing buffer at the specified offset
void copyChunkToProcessingBuffer(estimationBuffer &destination, const pulseData &source, size_t offset) {
    memcpy(destination.sample_buffer_Ir + offset, 
        source.sample_buffer_Ir, CHUNK_SIZE * sizeof(int32_t));
    memcpy(destination.sample_buffer_Red + offset, 
        source.sample_buffer_Red, CHUNK_SIZE * sizeof(int32_t));

    memcpy(destination.sample_buffer_AccX + offset, 
        source.sample_buffer_AccX, CHUNK_SIZE * sizeof(int32_t));
    memcpy(destination.sample_buffer_AccY + offset, 
        source.sample_buffer_AccY, CHUNK_SIZE * sizeof(int32_t));
    memcpy(destination.sample_buffer_AccZ + offset, 
        source.sample_buffer_AccZ, CHUNK_SIZE * sizeof(int32_t));
}

// shift the existing data to make room for the newest chunk
void shiftProcessingBuffer(estimationBuffer &buffer, size_t data_size) {
    memmove(buffer.sample_buffer_Ir,
        buffer.sample_buffer_Ir + CHUNK_SIZE, data_size * sizeof(int32_t));
    memmove(buffer.sample_buffer_Red,
        buffer.sample_buffer_Red + CHUNK_SIZE, data_size * sizeof(int32_t));

    memmove(buffer.sample_buffer_AccX,
        buffer.sample_buffer_AccX + CHUNK_SIZE, data_size * sizeof(int32_t));
    memmove(buffer.sample_buffer_AccY,
        buffer.sample_buffer_AccY + CHUNK_SIZE, data_size * sizeof(int32_t));
    memmove(buffer.sample_buffer_AccZ,
        buffer.sample_buffer_AccZ + CHUNK_SIZE, data_size * sizeof(int32_t));
}