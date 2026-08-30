#ifndef COLLECTOR_CONTEXT_H
#define COLLECTOR_CONTEXT_H

#include <freertos/FreeRTOS.h>
#include "signal_channel.h"

struct SystemContext;

struct CollectorState {
    int buffer_idx = 0;
    int spo2_idx = 0;
    bool first_sample = true;

    bool is_finger_removed = false;
    TickType_t finger_removed_time = 0;

    int no_motion_counter = 0;
    int pulsation_no_signal_counter = 0;
    int pulsation_signal_counter = 0;
    int PULSATION_DELAY = 3000;
};

struct CollectorFilters {
    ChannelFilter ir;
    ChannelFilter red;
    ChannelFilter accX;
    ChannelFilter accY;
    ChannelFilter accZ;
};

void initCollectorFilters(FilterAlgorithms &filter, CollectorFilters &filters);
void resetMeasurementSession(SystemContext &sysCtx, CollectorState &state);

#endif