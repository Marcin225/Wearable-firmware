#ifndef SIGNAL_CHANNEL_H
#define SIGNAL_CHANNEL_H

#include "post_processor.h"
#include "pre_processor.h"
#include "../../include/config.h"

struct ChannelFilter {
    biquadFilter lowPass{};
    biquadFilter highPass{};
    int32_t medianBuffer[2] = {0};
};

void initChannel(FilterAlgorithms& filter, ChannelFilter& channel);
int32_t processChannel(FilterAlgorithms& filter, ChannelFilter& channel, int32_t sample, bool firstSample);
void shiftDcSignalSum(int64_t *signalSum);

enum class BufferWarmupStage {
    EMPTY = 0,
    QUARTER_FULL = 1,
    HALF_FULL = 2,
    THREE_QUARTERS_FULL = 3,
    READY = 4
};

extern bool buffer_ready;

extern BufferWarmupStage fill_stage;

#endif