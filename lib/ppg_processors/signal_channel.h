#ifndef SIGNAL_CHANNEL_H
#define SIGNAL_CHANNEL_H

#include <stdint.h>
#include "pre_processor.h"

struct ChannelFilter {
    biquadFilter lowPass{};
    biquadFilter highPass{};
    int32_t medianBuffer[2] = {0};
};

void initChannel(FilterAlgorithms &filter, ChannelFilter &channel);
int32_t processChannel(FilterAlgorithms &filter, ChannelFilter &channel, int32_t sample, bool firstSample);

#endif