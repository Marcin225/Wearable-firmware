#include "signal_channel.h"

// initialize the Butterworth band-pass filter for the frequency range 0.4 - 4 (Hz)
// coefficients are stored in Q2.30 fixed-point format
void initChannel(FilterAlgorithms &filter, ChannelFilter &channel) {
    filter.initFilter(&channel.lowPass, 11803882, 23607764, 11803882, -1830343161, 806667139); // low pass
    filter.initFilter(&channel.highPass, 1073741824, -2147483648, 1073741824, -2110933440, 1037980441); // high pass
}


// apply median filtering followed by the Butterworth band-pass filter
// initialize the filter state from the first sample to avoid startup transients
int32_t processChannel(FilterAlgorithms &filter, ChannelFilter &channel, int32_t sample, bool firstSample) {
    if (firstSample) {
        channel.medianBuffer[0] = sample;
        channel.medianBuffer[1] = sample;
    }

    int32_t medianSample = filter.medianFilter(sample, channel.medianBuffer);

    if (firstSample) {
        filter.initBandPassSteadyState(&channel.lowPass, &channel.highPass, medianSample);
    }

    int32_t filteredSample = filter.bandPassFilter(&channel.lowPass, medianSample);
    filteredSample = filter.bandPassFilter(&channel.highPass, filteredSample);

    return filteredSample;
}