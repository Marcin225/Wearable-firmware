#include "algorithm_NLMS.h"

void NLMS(int32_t *referenceNoise, int32_t *filterWeights, int32_t *ACData, 
        int32_t mu, int32_t eps, const int numOfTaps, const int bufferSize, int32_t *noiseHistory) {
    
    const int SHIFT = 15;                   // Q15
    const int M = numOfTaps;
    
    for (int n = 0; n < bufferSize; n++) {
        int64_t estimatedNoise = 0;
        int64_t power = 0;

        // update and shift noise history
        for (int i = M - 1; i > 0; i--) {
            noiseHistory[i] = noiseHistory[i - 1];
        }
        // Store newest sample in history
        noiseHistory[0] = referenceNoise[n];

        for (int k = 0; k < M; k++) {
            estimatedNoise += ((int64_t)filterWeights[k] * noiseHistory[k]);
            power += (int64_t)noiseHistory[k] * noiseHistory[k];
        }

        estimatedNoise >>= SHIFT;

        int32_t errorSignal = ACData[n] - (int32_t)estimatedNoise; // estimation error (residual signal after noise cancellation)
        ACData[n] = errorSignal; // filtered (denoised) output signal

        // Normalize adaptation to signal energy
        int64_t denominator = power + eps;
        if (denominator <= 0) {
            continue;
        }
        // adapt filter weights
        for (int k = 0; k < M; k++) {
            int64_t delta = (((int64_t)errorSignal * noiseHistory[k] * mu) << SHIFT) / denominator;
            delta >>= SHIFT;
            
            filterWeights[k] += (int32_t)delta;
        }
    }
}