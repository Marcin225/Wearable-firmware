#ifndef PPG_PROCESSOR_H
#define PPG_PROCESSOR_H

#define MAX_HR_DIFF 6 // maximum allowed HR difference between consecutive chunks
#define HR_SMOOTH_ALPHA 71 // HR smoothing coefficient in Q8 fixed-point format

#include <stdint.h>

#include "../../include/config.h"
#include "processor_types.h"
#include "measurement_types.h"

struct SignalProcessingTestAccess;

class SignalProcessingAlgorithms {
public:
    SignalProcessingAlgorithms();

    VitalResult calculateVitals(int32_t bonusQ12, int32_t mainPenaltyQ12, int32_t thCfQ12);
    int32_t smooth_hr(int32_t hr);
    void reset_session();

    uint8_t getState() const {
        return StateMachine.state;
    }

    spo2 spo2Data{};
    estimationBuffer processBuffer{};

private:
    friend struct SignalProcessingTestAccess;

    void process_rfft(int32_t *signal, int32_t *re, int32_t *im, int N = FFT_SIZE);
    void process_single_bin_fft(int32_t *signal, int32_t *temp_buffer, int bin, int N);

    void calculate_hr_candidates(int32_t *re, int32_t *im);
    void calculate_motion_frequencies(int32_t *re_1, int32_t *im_1, int32_t *re_2, int32_t *im_2, int32_t *re_3, int32_t *im_3);

    int calculate_hr(int32_t bonus_weight, int32_t main_penalty_weight, int32_t th_cf);
    int calculate_spo2();

    int64_t get_max_motion_penalty_bin(int bin);
    int32_t normalize_power(int64_t power, int64_t max_power, int64_t min_power);

    hrCandidatesNorm HrTopCandidates{};
    motionNorm motionHrBand{};
    FSM StateMachine{};
    fftWorkspace sharedFftBuffer{};

    int32_t display_hr = 0;
};

#endif