#include "spo2_dc.h"
#include "../../include/config.h"

void shiftDcSignalSum(int64_t *signalSum) {
    for (int i = 0; i < 3; i++) {
        signalSum[i] = signalSum[i + 1];
    }

    signalSum[3] = 0;
}

// calculate a rolling DC estimate using four partial sums
// this avoids rescanning the complete PPG window for every SpO2 calculation
void updateSpo2Dc(spo2 &spo2Data, int &spo2_idx, uint32_t ir, uint32_t red) {
    if (spo2_idx >= BUFFER_SIZE - CHUNK_SIZE) {
        spo2Data.signal_sum_Ir[3] += ir;
        spo2Data.signal_sum_Red[3] += red;
        if (spo2_idx >= BUFFER_SIZE - 1) {

            int64_t sumIr = 0;
            int64_t sumRed = 0;
            for (int s = 0; s < 4; s++) {
                sumIr += spo2Data.signal_sum_Ir[s];
                sumRed += spo2Data.signal_sum_Red[s];
            }

            spo2Data.dcIr = sumIr / BUFFER_SIZE;
            spo2Data.dcRed = sumRed / BUFFER_SIZE;

            shiftDcSignalSum(spo2Data.signal_sum_Ir);
            shiftDcSignalSum(spo2Data.signal_sum_Red);

            spo2_idx = BUFFER_SIZE - CHUNK_SIZE - 1;
        }
    }else if (spo2_idx >= 2 * CHUNK_SIZE && spo2_idx < BUFFER_SIZE - CHUNK_SIZE) {
        spo2Data.signal_sum_Ir[2] += ir;
        spo2Data.signal_sum_Red[2] += red;
    }else if (spo2_idx >= CHUNK_SIZE && spo2_idx <  2 * CHUNK_SIZE) {
        spo2Data.signal_sum_Ir[1] += ir;
        spo2Data.signal_sum_Red[1] += red;
    }else if (spo2_idx >= 0 && spo2_idx < CHUNK_SIZE) {
        spo2Data.signal_sum_Ir[0] += ir;
        spo2Data.signal_sum_Red[0] += red;
    }
}