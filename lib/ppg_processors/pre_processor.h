#ifndef PRE_PROCESSOR_H
#define PRE_PROCESSOR_H

#include <stdint.h>

// Butterworth band-pass biquad section with Q2.30 coefficients
typedef struct {
    int32_t b0, b1, b2, a1, a2;

    int32_t x1 = 0;
    int32_t x2 = 0;
    int32_t y1 = 0;
    int32_t y2 = 0;

} biquadFilter;

class FilterAlgorithms {
    public:
        FilterAlgorithms(void);
        
        static int32_t medianFilter(int32_t current, int32_t *buffer);
        static int32_t bandPassFilter(biquadFilter *filter, int32_t sample);
        void initFilter(biquadFilter *filter, int32_t b0, int32_t b1, int32_t b2, int32_t a1, int32_t a2);
        void initBandPassSteadyState(biquadFilter *lp_filter, biquadFilter *hp_filter, int32_t sample);

};

#endif