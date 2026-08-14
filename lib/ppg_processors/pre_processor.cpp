#include "pre_processor.h"

FilterAlgorithms::FilterAlgorithms(void) { }


int32_t FilterAlgorithms::medianFilter(int32_t current, int32_t *buffer) {
    int32_t v[3] = {current, buffer[0], buffer[1]};

    buffer[1] = buffer[0];
    buffer[0] = current;

    if (v[0] > v[1]) {int32_t t = v[0]; v[0] = v[1]; v[1] = t;}
    if (v[1] > v[2]) {int32_t t = v[1]; v[1] = v[2]; v[2] = t;}
    if (v[0] > v[1]) {int32_t t = v[0]; v[0] = v[1]; v[1] = t;}

    return v[1];
}


void FilterAlgorithms::initFilter(biquadFilter *filter, int32_t b0, int32_t b1, int32_t b2, int32_t a1, int32_t a2) {
    filter->b0 = b0;
    filter->b1 = b1;
    filter->b2 = b2;

    filter->a1 = a1;
    filter->a2 = a2;
}


void FilterAlgorithms::initBandPassSteadyState(biquadFilter *lp_filter, biquadFilter *hp_filter, int32_t sample) {
    int64_t num = (int64_t)lp_filter->b0 + lp_filter->b1 + lp_filter->b2;
    int64_t den = (1LL << 30) + lp_filter->a1 + lp_filter->a2;

    int32_t initOutput = (int32_t)(((int64_t)sample * num) / den);

    lp_filter->x1 = sample;
    lp_filter->x2 = sample;
    lp_filter->y1 = initOutput;
    lp_filter->y2 = initOutput;

    hp_filter->x1 = initOutput;
    hp_filter->x2 = initOutput;
    hp_filter->y1 = 0;
    hp_filter->y2 = 0;
}


// Butterworth bandpass Filter
int32_t FilterAlgorithms::bandPassFilter(biquadFilter *filter, int32_t sample) {
    int64_t accum = 0;
    accum = (int64_t)filter->b0 * sample +
            (int64_t)filter->b1 * filter->x1 +
            (int64_t)filter->b2 * filter->x2 -
            (int64_t)filter->a1 * filter->y1 -
            (int64_t)filter->a2 * filter->y2;

    filter->x2 = filter->x1;
    filter->x1 = sample;
    filter->y2 = filter->y1;

    int32_t output = (accum + (1LL << 29)) >> 30;

    filter->y1 = output;

    return output;
}