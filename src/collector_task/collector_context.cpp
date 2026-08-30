#include "collector_context.h"
#include "SystemContext.h"

void initCollectorFilters(FilterAlgorithms &filter, CollectorFilters &filters) {
    initChannel(filter, filters.ir);
    initChannel(filter, filters.red);
    initChannel(filter, filters.accX);
    initChannel(filter, filters.accY);
    initChannel(filter, filters.accZ);
}

// reset acquisition state after loss of signal continuity
void resetMeasurementSession(SystemContext &sysCtx, CollectorState &state) {
    state.buffer_idx = 0;
    state.spo2_idx = 0;
    state.first_sample = true;

    // reset DC accumulation for the new continuous measurement window
    for (int s = 0; s < 4; s++) {
        sysCtx.algorithm.spo2Data.signal_sum_Ir[s] = 0;
        sysCtx.algorithm.spo2Data.signal_sum_Red[s] = 0;
    }

    // increment the session ID to invalidate chunks from the previous session
    sysCtx.measurementSessionId.fetch_add(1, std::memory_order_relaxed);
}