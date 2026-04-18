#ifndef ALGORITHM_NLMS_H
#define ALGORITHM_NLMS_H

#include <stdint.h>

void NLMS(int32_t *referenceNoise, int32_t *filterWeights, int32_t *ACData, 
    int32_t mu, int32_t eps, const int NLMS_NUM_OF_TAPS, const int bufferSize, int32_t *noiseHistory);

#endif