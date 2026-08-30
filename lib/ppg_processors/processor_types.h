#ifndef PROCESSOR_TYPES_H
#define PROCESSOR_TYPES_H

#include <stdint.h>
#include "../../include/config.h"

#define MAX_CANDIDATES 4

// shared FFT buffers reused during HR and motion spectrum calculations
struct fftWorkspace {
    int32_t re_1[SPECTRUM_SIZE];
    int32_t im_1[SPECTRUM_SIZE];

    int32_t re_2[SPECTRUM_SIZE];
    int32_t im_2[SPECTRUM_SIZE];

    int32_t re_3[SPECTRUM_SIZE];
    int32_t im_3[SPECTRUM_SIZE];
};

// stores the strongest HR candidates and values used for their final scoring
struct hrCandidatesNorm {
    int64_t power[MAX_CANDIDATES];
    int32_t th_cf[MAX_CANDIDATES];
    uint16_t frequency[MAX_CANDIDATES];
    uint8_t index[MAX_CANDIDATES];
    int64_t score[MAX_CANDIDATES];
};

// stores the normalized motion spectrum in the HR frequency range
struct motionNorm {
    int64_t power[55];
    uint16_t frequency[55];
};

// state used to stabilize HR estimation between consecutive analysis windows
struct FSM {
    uint8_t state;
    uint8_t alertCounter;
    uint8_t recovery_counter;
    uint8_t good_windows;
    uint8_t last_stable_hr;
    uint8_t last_hr;
    uint8_t second_last_hr;
};

// stores AC power and rolling four-part DC sums used for SpO2 estimation
struct spo2 {
    int64_t power_acIr[4];
    int64_t power_acRed;
    int32_t dcIr;
    int32_t dcRed;
    int bin;
    int64_t signal_sum_Ir[4];
    int64_t signal_sum_Red[4];
};

// final values returned by one processing cycle
struct VitalResult {
    int32_t heartRate = 0;
    int32_t spo2 = 0;
};

#endif