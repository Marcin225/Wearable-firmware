#include "pre_processor.h"

FilterAlgorithms::FilterAlgorithms(void) { }


int32_t FilterAlgorithms::medianFilter(int32_t current, int32_t *buffer) {
    int32_t v[3] = {current, buffer[0], buffer[1]};

    if (v[0] > v[1]) {int32_t t = v[0]; v[0] = v[1]; v[1] = t;}
    if (v[1] > v[2]) {int32_t t = v[1]; v[1] = v[2]; v[2] = t;}
    if (v[0] > v[1]) {int32_t t = v[0]; v[0] = v[1]; v[1] = t;}

    return v[1];
}


int32_t FilterAlgorithms::absValueOf(int32_t x) {
    return (x < 0) ? -x : x;
}


// Butterworth bandpass Filter
int32_t FilterAlgorithms::bandPassFilter(biquadFilter *filter, int32_t sample) {
    int64_t accum = 0;
    accum = (int64_t)(filter->b0 * sample) + 
            (int64_t)(filter->b1 * filter->x1) +
            (int64_t)(filter->b2 * filter->x2) -
            (int64_t)(filter->a1 * filter->y1) -
            (int64_t)(filter->a2 * filter->y2);
                        
    filter->x2 = filter->x1;
    filter->x1 = sample;
    filter->y2 = filter->y1;

    int32_t output = (int32_t)(accum >> 30);

    filter->y1 = output;

    return output;
}