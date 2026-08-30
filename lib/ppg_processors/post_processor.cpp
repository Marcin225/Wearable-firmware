#include "post_processor.h"

#include "algorithm_RFFT.h"
#include "fft_tables.h"

SignalProcessingAlgorithms::SignalProcessingAlgorithms() {}

// reset the analysis state while preserving the DC accumulator managed by the collector task
// reset only the SpO2 analysis fields explicitly to avoid clearing the DC accumulator
void SignalProcessingAlgorithms::reset_session() {
    HrTopCandidates = {};
    motionHrBand = {};
    StateMachine = {};

    spo2Data.power_acIr[0] = 0;
    spo2Data.power_acIr[1] = 0;
    spo2Data.power_acIr[2] = 0;
    spo2Data.power_acIr[3] = 0;
    spo2Data.power_acRed = 0;
    spo2Data.bin = 0;

    display_hr = 0;
}

// run one complete analysis cycle on the current 1024-sample window
// HR is estimated first because the selected HR bin is also used for the red-channel SpO2 calculation
VitalResult SignalProcessingAlgorithms::calculateVitals(int32_t bonusQ12, int32_t mainPenaltyQ12, int32_t thCfQ12) {
    VitalResult result;

    process_rfft(processBuffer.sample_buffer_Ir, sharedFftBuffer.re_1, sharedFftBuffer.im_1, FFT_SIZE);
    calculate_hr_candidates(sharedFftBuffer.re_1, sharedFftBuffer.im_1);

    process_rfft(processBuffer.sample_buffer_AccX, sharedFftBuffer.re_1, sharedFftBuffer.im_1, FFT_SIZE);
    process_rfft(processBuffer.sample_buffer_AccY, sharedFftBuffer.re_2, sharedFftBuffer.im_2, FFT_SIZE);
    process_rfft(processBuffer.sample_buffer_AccZ, sharedFftBuffer.re_3, sharedFftBuffer.im_3, FFT_SIZE);

    calculate_motion_frequencies(sharedFftBuffer.re_1, sharedFftBuffer.im_1,
        sharedFftBuffer.re_2, sharedFftBuffer.im_2,
        sharedFftBuffer.re_3, sharedFftBuffer.im_3);

    result.heartRate = calculate_hr(bonusQ12, mainPenaltyQ12, thCfQ12);

    process_single_bin_fft(processBuffer.sample_buffer_Red, sharedFftBuffer.re_1, spo2Data.bin, FFT_SIZE);

    if (result.heartRate > 0 && StateMachine.state == 0) {
        result.spo2 = calculate_spo2();
    }else {
        result.spo2 = 0;
    }

    return result;
}

// remove the DC component, apply a Hann window and perform a zero-padded real FFT (for finer frequency resolution)
void SignalProcessingAlgorithms::process_rfft(int32_t *signal, int32_t *re, int32_t *im, int N) {

    int M = N / 2;
    rfftAlgorithm rfft;

    int64_t sum = 0;
    for (int i = 0; i < M; i++) {
        sum += signal[i];
    }
    int32_t signal_mean = sum / M;

    // remove signal mean, apply hann window and split into even (re) and odd (im) samples
    for (int i = 0; i < M / 2; i++) {
        int even_idx = i * 2;
        int odd_idx = i * 2 + 1;

        int64_t even = ((int64_t)signal[even_idx] - signal_mean) << 15;
        int64_t odd = ((int64_t)signal[odd_idx] - signal_mean) << 15;

        re[i] = (int32_t)((even * hann_table_q31[even_idx] + (1LL << 30)) >> 31); // (signal - mean) * hann_window
        im[i] = (int32_t)((odd * hann_table_q31[odd_idx] + (1LL << 30)) >> 31);
    }

    // zero-padding for improved frequency resolution
    for (int i = M / 2; i < M; i++) {
        re[i] = 0;
        im[i] = 0;
    }

    rfft.rfft(re, im, N); // returns fft spectrum (output in re and im arrays)

}