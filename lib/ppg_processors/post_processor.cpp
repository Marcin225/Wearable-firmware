#include "post_processor.h"

SignalProcessingAlgorithms::SignalProcessingAlgorithms() {}

void SignalProcessingAlgorithms::reset_session() {
    HrTopCandidates = {};
    motionHrBand = {};
    StateMachine = {};
    // spo2Data = {};

    spo2Data.power_acIr[0] = 0;
    spo2Data.power_acIr[1] = 0;
    spo2Data.power_acIr[2] = 0;
    spo2Data.power_acIr[3] = 0;
    spo2Data.power_acRed = 0;
    spo2Data.bin = 0;
    display_hr = 0;
}

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

void SignalProcessingAlgorithms::process_rfft(int32_t *signal, int32_t *re, int32_t *im, int N) {

    int M = N / 2;
    rfftAlgorithm rfft;

    // calculate signal mean
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


void SignalProcessingAlgorithms::process_single_bin_fft(int32_t *signal, int32_t *temp_buffer, int bin, int N) {
    int M = N / 2;
    rfftAlgorithm single_bin_fft;

    // calculate signal mean
    int64_t sum = 0;
    for (int i = 0; i < M; i++) {
        sum += signal[i];
    }
    int32_t signal_mean = sum / M;

    for (int i = 0; i < M; i++) {
        int64_t dcOff = ((int64_t)signal[i] - signal_mean) << 15;

        temp_buffer[i] = (int32_t)((dcOff * hann_table_q31[i] + (1LL << 30)) >> 31); // (signal - mean) * hann_window
    }

    int64_t power = single_bin_fft.calculate_single_bin_power(temp_buffer, bin, N);

    spo2Data.power_acRed = power;
}


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

    int spo2 = spo2_table[ratio];

    return spo2;
}

int32_t SignalProcessingAlgorithms::normalize_power(int64_t power, int64_t max_power, int64_t min_power) {
    uint64_t range = max_power - min_power;
    int shift = 0;

    while (range > 8589934592ULL) {
        range >>= 1;
        shift++;
    }

    if (range == 0) {
        range = 1;
    }

    uint64_t shift_power = (power - min_power) >> shift;

    int32_t norm_power = (int32_t)(shift_power * 2147483647ULL / range);

    return norm_power;
}


void SignalProcessingAlgorithms::calculate_hr_candidates(int32_t *re, int32_t *im) {

    // hr band 0.67 hz - 3.3 hz ->  ≈ 40 - 200 bpm | for fft frequency resolution = 100 hz / 2048 
    int64_t max_power = -9223372036854775807;
    int64_t min_power = 9223372036854775807;
    int64_t mean_power = 0;
    int candidate_idx = 0;

    uint64_t accum = 0;
    for (int i = 14; i <= 68; i++) {
        accum += (int64_t)re[i] * re[i] + (int64_t)im[i] * im[i];
    }

    mean_power = ((accum + 1) >> 1) / 55; // 55 = 68 - 14 + 1
    int64_t threshold = mean_power * 2;
    HrTopCandidates = {0};

    for (int i = 14; i <= 68; i++) {
        int64_t current_power = (((int64_t)re[i] * re[i] + (int64_t)im[i] * im[i]) + 1 ) >> 1;

        int64_t left_power = (((int64_t)re[i-1] * re[i-1] + (int64_t)im[i-1] * im[i-1]) + 1) >> 1;
        int64_t right_power = (((int64_t)re[i+1] * re[i+1] + (int64_t)im[i+1] * im[i+1]) + 1) >> 1;

        if (current_power > max_power) {
            max_power = current_power;
        }
        if (current_power < min_power) {
            min_power = current_power;
        }

        if (current_power >= right_power && current_power >= left_power && current_power >= threshold) {
            int insert_idx = MAX_CANDIDATES;

            for (int k = 0; k < MAX_CANDIDATES; k++) {
                if (current_power > HrTopCandidates.power[k]) {
                    insert_idx = k;
                    break;
                }
            }

            if (insert_idx < MAX_CANDIDATES) {
                for (int k = MAX_CANDIDATES - 1; k > insert_idx; k--) {
                    HrTopCandidates.power[k] = HrTopCandidates.power[k-1];
                    HrTopCandidates.frequency[k] = HrTopCandidates.frequency[k-1];
                    HrTopCandidates.index[k] = HrTopCandidates.index[k-1];
                }

                HrTopCandidates.power[insert_idx] = current_power;
                HrTopCandidates.frequency[insert_idx] = i * 100  * 16384 / 2048;
                HrTopCandidates.index[insert_idx] = i;
            }
        }
    }

    if (max_power == min_power)
        return;

    int32_t mean_power_norm = normalize_power(mean_power, max_power, min_power);

    if (mean_power_norm == 0)
        mean_power_norm = 1;

    for (int i = 0; i < MAX_CANDIDATES ; i++) { // number of hr candidates
        if (HrTopCandidates.frequency[i] > 0) {
            spo2Data.power_acIr[i] = HrTopCandidates.power[i];
            HrTopCandidates.power[i] = normalize_power(HrTopCandidates.power[i], max_power, min_power);
            // HrTopCandidates.power[i] = (int64_t)((int128_t)(HrTopCandidates.power[i] - min_power) * 2147483647 / (max_power - min_power));
            HrTopCandidates.th_cf[i] = HrTopCandidates.power[i] * 4096 / mean_power_norm;
        }
    }
}


void SignalProcessingAlgorithms::calculate_motion_frequencies(int32_t *re_1, int32_t *im_1,
                                                            int32_t *re_2, int32_t *im_2,
                                                            int32_t *re_3, int32_t *im_3) {

    int64_t sum_power[55] = {0};
    int64_t max_power = -9223372036854775807;
    int64_t min_power = 9223372036854775807;

    for (int i = 14; i <= 68; i++) {
        int64_t power_x = (((int64_t)re_1[i] * re_1[i] + (int64_t)im_1[i] * im_1[i]) + 1) >> 1;

        int64_t power_y = (((int64_t)re_2[i] * re_2[i] + (int64_t)im_2[i] * im_2[i]) + 1) >> 1;

        int64_t power_z = (((int64_t)re_3[i] * re_3[i] + (int64_t)im_3[i] * im_3[i]) + 1) >> 1;


        sum_power[i-14] = power_x + power_y + power_z;

        if (sum_power[i-14] > max_power) {
            max_power = sum_power[i-14];
        }
        if (sum_power[i-14] < min_power) {
            min_power = sum_power[i-14];
        }

    }

    if (max_power == min_power) {
        for (int i = 0; i < 55; i++) {
            motionHrBand.power[i] = 0;
            motionHrBand.frequency[i] = (i + 14) * 100 * 16384 / 2048;
        }
        return;
    }


    for (int i = 0; i < 55; i++) {
        motionHrBand.power[i] = normalize_power(sum_power[i], max_power, min_power);
        // motionHrBand.power[i] = (int64_t)((int128_t)(sum_power[i] - min_power) * 2147483647 / (max_power - min_power));
        motionHrBand.frequency[i] = (i + 14) * 100  * 16384 / 2048;
    }   
}


int32_t SignalProcessingAlgorithms::smooth_hr(int32_t hr) {
    if (hr <= 0) {
        return 0;
    }

    if (display_hr <= 0) {
        display_hr = hr;
    }else {
        display_hr += (HR_SMOOTH_ALPHA * (hr - display_hr) + 128) >> 8;
    }

    return display_hr;
}


int64_t SignalProcessingAlgorithms::get_max_motion_penalty_bin(int bin) {
    int64_t max_motion = 0;
    int imu_idx = bin - 14;

    int start = imu_idx - 2;
    int end = imu_idx + 2;

    if (start < 0) {
        start = 0;
    }
    if (end > 54) {
        end = 54;
    }

    for (int i = start; i <= end; i++) {
        if (motionHrBand.power[i] > max_motion) {
            max_motion = motionHrBand.power[i];
        }
    }

    return max_motion;
}


int SignalProcessingAlgorithms::calculate_hr(int32_t bonus_weight, int32_t main_penalty_weight, int32_t th_cf) {
    int winner_idx = -1;
    int64_t max_score = -9223372036854775807;

    for (int c = 0; c < MAX_CANDIDATES; c++) { // calculate candidate score
        if (HrTopCandidates.power[c] <= 0) {
            continue;
        }

        int candidate_hr = (HrTopCandidates.frequency[c] * 60 + (1LL << 13)) >> 14;

        int imu_idx = HrTopCandidates.index[c];

        int64_t bonus = 0;
        int64_t max_motion = 0;

        max_motion = get_max_motion_penalty_bin(imu_idx);

        int64_t main_bin_penalty = (max_motion * main_penalty_weight + (1LL << 11)) >> 12;

        if (StateMachine.last_stable_hr > 0) {
            int diff_abs = absValueOf(candidate_hr - StateMachine.last_stable_hr);

            if (diff_abs < 15) {
                int64_t bonus_q12 = (int64_t)(15 - diff_abs) * bonus_weight;
                bonus = (bonus_q12 * 2147483647LL + (1LL << 11)) >> 12;
            }
        }

        HrTopCandidates.score[c] = HrTopCandidates.power[c] - main_bin_penalty + bonus;

        if (HrTopCandidates.score[c] > max_score) {
            max_score = HrTopCandidates.score[c];
            winner_idx = c;
        }
    }

    if (winner_idx < 0) {
        return 0;
    }

    spo2Data.power_acIr[0] = spo2Data.power_acIr[winner_idx];
    spo2Data.bin = HrTopCandidates.index[winner_idx];

    int32_t th_cf_winner = HrTopCandidates.th_cf[winner_idx];
    int hr_winner = (HrTopCandidates.frequency[winner_idx] * 60 + (1LL << 13)) >> 14;

    bool is_singal_sharp = th_cf_winner > th_cf;
    int hr_jump = absValueOf(hr_winner - StateMachine.last_stable_hr);
    int recovery_jump = absValueOf(hr_winner - StateMachine.last_hr);
    int second_recovery_jump = absValueOf(StateMachine.last_hr - StateMachine.second_last_hr);

    switch (StateMachine.state) {
        case 0: // STABLE
            if (is_singal_sharp && (hr_jump <= MAX_HR_DIFF || StateMachine.last_stable_hr == 0)) {
                StateMachine.last_stable_hr = hr_winner;
                StateMachine.second_last_hr = StateMachine.last_hr;
                StateMachine.last_hr = hr_winner;

                return StateMachine.last_stable_hr;
            } else {
                StateMachine.state = 1; // ALERT
                StateMachine.alertCounter = 0;
                StateMachine.second_last_hr = StateMachine.last_hr;
                StateMachine.last_hr = hr_winner;

                return StateMachine.last_stable_hr;
            }

        case 1: // ALERT
            if (is_singal_sharp) {
                StateMachine.state = 3; // RECOVERY
                StateMachine.recovery_counter = 0;
                StateMachine.second_last_hr = StateMachine.last_hr;
                StateMachine.last_hr = hr_winner;

                return StateMachine.last_stable_hr;
            } else {
                StateMachine.alertCounter++;
                if (StateMachine.alertCounter >= 3) {
                    StateMachine.state = 2; // UNCERTAIN
                    StateMachine.alertCounter = 0;

                    return 0;
                }

                return StateMachine.last_stable_hr;
            }

        case 2: // UNCERTAIN
            if (is_singal_sharp) {
                StateMachine.good_windows++;
                if (StateMachine.good_windows >= 2) {
                    StateMachine.state = 1;
                    StateMachine.good_windows = 0;

                    return 0;
                }
                return 0;
            }else {
                StateMachine.good_windows = 0;

                return 0;
            }

        case 3: // RECOVERY
            if (is_singal_sharp) {
                StateMachine.recovery_counter++;
                if (StateMachine.recovery_counter >= 3) {
                    if (recovery_jump <= MAX_HR_DIFF && second_recovery_jump <= MAX_HR_DIFF) {
                        StateMachine.state = 0; // STABLE
                        StateMachine.last_stable_hr = hr_winner;
                        StateMachine.second_last_hr = StateMachine.last_hr;
                        StateMachine.last_hr = hr_winner;
                        StateMachine.recovery_counter = 0;

                        return StateMachine.last_stable_hr;
                    }
                    StateMachine.recovery_counter = 0;
                }
                StateMachine.second_last_hr = StateMachine.last_hr;
                StateMachine.last_hr = hr_winner;

                return StateMachine.last_stable_hr;
            }else {
                StateMachine.state = 1;
                StateMachine.recovery_counter = 0;

                return StateMachine.last_stable_hr;
            }
    }
}