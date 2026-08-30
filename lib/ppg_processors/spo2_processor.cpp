#include "post_processor.h"

#include "algorithm_RFFT.h"
#include "fft_tables.h"
#include "common.h"

// calculate the Red-channel AC power at the selected HR frequency bin for SpO2 estimation
// apply DC removal and a Hann window before calculating power
void SignalProcessingAlgorithms::process_single_bin_fft(int32_t *signal, int32_t *temp_buffer, int bin, int N) {
    int M = N / 2;
    rfftAlgorithm single_bin_fft;

    int64_t sum = 0;

    for (int i = 0; i < M; i++) {
        sum += signal[i];
    }

    int32_t signal_mean = sum / M;

    for (int i = 0; i < M; i++) {
        int64_t dcOff = ((int64_t)signal[i] - signal_mean) << 15;
        temp_buffer[i] = (int32_t)((dcOff * hann_table_q31[i] + (1LL << 30)) >> 31);
    }

    spo2Data.power_acRed = single_bin_fft.calculate_single_bin_power(temp_buffer, bin, N);
}

// calculate SpO2 using the ratio-of-ratios method and the MAX30102 lookup table
int SignalProcessingAlgorithms::calculate_spo2() {
    uint32_t AcIr = isqrt(spo2Data.power_acIr[0]);
    uint32_t AcRed = isqrt(spo2Data.power_acRed);

    int64_t num = (int64_t)AcRed * spo2Data.dcIr * 100;
    int64_t den = (int64_t)AcIr * spo2Data.dcRed;

    if (den == 0) {
        return 0;
    }

    int32_t ratio = num / den;

    if (ratio < 20 || ratio > 150) {
        return 0;
    }

    return spo2_table[ratio];
}