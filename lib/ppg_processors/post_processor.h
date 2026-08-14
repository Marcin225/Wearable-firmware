#ifndef PPG_PROCESSOR_H
#define PPG_PROCESSOR_H

#define MAX_CANDIDATES          4
#define MAX_HR_DIFF             6
#define HR_SMOOTH_ALPHA         71 //Q8

#include <stdint.h>
#include <cstring>
#include "algorithm_RFFT.h"
#include "../utils/fft_tables.h"
#include "../utils/common.h"

typedef struct {
    int64_t power[MAX_CANDIDATES]; // Q31 notation
    int32_t th_cf[MAX_CANDIDATES]; // Q31 notation
    uint16_t frequency[MAX_CANDIDATES]; // Q 2.14 notation
    uint8_t index[MAX_CANDIDATES];
    int64_t score[MAX_CANDIDATES];

} hrCandidatesNorm;

typedef struct {
    int64_t power[55]; // 68 - 14 + 1 (14 - > 0.67 Hz - 3.3 Hz <- 68)
    uint16_t frequency[55];
} motionNorm;

typedef struct {
    uint8_t state; // 0 - "STABLE" | 1 - "ALERT" | 2 - "UNCERTAIN" | 3 - "RECOVERY"
    uint8_t alertCounter;
    uint8_t recovery_counter;
    uint8_t good_windows;
    uint8_t last_stable_hr;
    uint8_t last_hr;
    uint8_t second_last_hr;
} FSM;

typedef struct {
    int64_t power_acIr[4];
    int64_t power_acRed;
    int32_t dcIr;
    int32_t dcRed;
    int bin;
    int64_t signal_sum_Ir[4];
    int64_t signal_sum_Red[4];
} spo2;

class SignalProcessingAlgorithms {
public:
    SignalProcessingAlgorithms(void);

    void process_rfft(int32_t *data, int32_t *re, int32_t *im, int N = 2048); // N = 2048 -> 1024 data + zero padding
    void process_single_bin_fft(int32_t *signal, int32_t *temp_buffer, int bin, int N);
    int64_t get_max_motion_penalty_bin(int bin);
    void calculate_hr_candidates(int32_t *re, int32_t *im);
    void calculate_motion_frequencies(int32_t *re_1, int32_t *im_1,
                                    int32_t *re_2, int32_t *im_2,
                                    int32_t *re_3, int32_t *im_3); // one re/im pair per accelerometer axis

    int calculate_hr(int32_t bonus_weight, int32_t main_penalty_weight, int32_t th_cf);
    int calculate_spo2();
    int32_t smooth_hr(int32_t hr);

    hrCandidatesNorm HrTopCandidates = {0};
    motionNorm motionHrBand = {0};

    FSM StateMachine = {0};

    spo2 spo2Data = {0};

private:
    int32_t display_hr = 0;
};


#endif