#include "signal_channel.h"

DeviceState system_mode = DeviceState::WORK;

void initChannel(FilterAlgorithms& filter, ChannelFilter& channel) {
    filter.initFilter(&channel.lowPass, 11803882, 23607764, 11803882, -1830343161, 806667139);
    filter.initFilter(&channel.highPass, 1073741824, -2147483648, 1073741824, -2110933440, 1037980441);
}

int32_t processChannel(FilterAlgorithms& filter, ChannelFilter& channel, int32_t sample, bool firstSample) {
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

void shiftDcSignalSum(int64_t *signalSum) {
    for (int i = 0; i < 3; i++) {
        signalSum[i] = signalSum[i+1];
    }
    signalSum[3] = 0;
}