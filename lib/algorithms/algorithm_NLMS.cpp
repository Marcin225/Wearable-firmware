#include "algorithm_NLMS.h"

void NLMS(int32_t *referenceNoise, int32_t *filterWeights, int32_t *ACData, 
        int32_t mu, int32_t eps, const int NLMS_NUM_OF_TAPS, const int bufferSize) {
    
    const int SHIFT = 15;                   // Q15
    const int M = NLMS_NUM_OF_TAPS;
    
    for (int n = M - 1; n < bufferSize; n++) {
        int64_t estimatedNoise = 0;
        int64_t power = 0;
        

        for (int k = 0; k < M; k++) {
            estimatedNoise += ((int64_t)filterWeights[k] * referenceNoise[n-k]);
            power += (int64_t)referenceNoise[n-k] * referenceNoise[n-k];
        }

        estimatedNoise >>= SHIFT;

        int32_t errorSignal = ACData[n] - (int32_t)estimatedNoise; // estimation error (residual signal after noise cancellation)
        ACData[n] = errorSignal; // filtered (denoised) output signal
        
        int64_t denominator = power + eps;
        if (denominator <= 0) {
            continue;
        }

        for (int k = 0; k < M; k++) {
            int64_t delta = (((int64_t)errorSignal * referenceNoise[n-k] * mu) << SHIFT) / denominator;
            delta >>= SHIFT;
            
            filterWeights[k] += (int32_t)delta;
        }
    }
}