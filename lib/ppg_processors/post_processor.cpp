#include "post_processor.h"

SignalProcessingAlgorithms::SignalProcessingAlgorithms() {}

void SignalProcessingAlgorithms::process_rfft(int32_t *signal, int32_t *re, int32_t *im, int N=2048) {

    int M = N / 2;
    rfftAlgorithm rfft;

    // calculate signal mean
    int64_t sum = 0;
    for (int i = 0; i < M; i++) {
        sum += signal[i];
    }
    int32_t signal_mean = sum / M;

    // remove signal mean, apply hann window and split into even (re) and odd (im) samples
    for (int i = 0; i < M; i++) {
        int even_idx = i * 2;
        int odd_idx = i * 2 + 1;

        re[i] = (int32_t)((int64_t)((signal[even_idx] - signal_mean) * hann_table_q31[even_idx] + (1LL << 30)) >> 31); // (signal - mean) * hann_window
        im[i] = (int32_t)((int64_t)((signal[odd_idx] - signal_mean) * hann_table_q31[odd_idx] + (1LL << 30)) >> 31);
    }

    // zero-padding for improved frequency resolution
    for (int i = 512; i <= M; i++) {
        re[i] = 0;
        im[i] = 0;
    }

    rfft.rfft(re, im, N); // returns fft spectrum (output in re and im arrays)

}


void SignalProcessingAlgorithms::calculate_hr_candidates(int32_t *re, int32_t *im) {

    // hr band 0.67 hz - 3.3 hz ->  ≈ 40 - 200 bpm | for fft frequency resolution = 100 hz / 2048 
    int64_t max_power = -9223372036854775808;
    int64_t min_power = 9223372036854775808;
    int64_t mean_power = 0;
    int candidate_idx = 0;

    __int128_t accum = 0;
    for (int i = 14; i <= 68; i++) {
        accum += (__int128_t)re[i] * re[i] + (__int128_t)im[i] * im[i];
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
            int64_t candidate_power = current_power;
            if (candidate_power > HrTopCandidates.power[MAX_CANDIDATES - 1]) {
                for (int k = 0; k < MAX_CANDIDATES - 1; k++) {

                    HrTopCandidates.power[k] = HrTopCandidates.power[k+1];
                    HrTopCandidates.frequency[k] = HrTopCandidates.frequency[k+1];
                    HrTopCandidates.index[k] = HrTopCandidates.index[k+1];
                }

                HrTopCandidates.power[MAX_CANDIDATES - 1] = current_power;
                HrTopCandidates.frequency[MAX_CANDIDATES - 1] = i * 100  * 16384 / 2048;
                HrTopCandidates.index[MAX_CANDIDATES - 1] = i;
            }
        }
    }

    if (max_power == min_power)
        return;

    int32_t mean_power_norm = (mean_power - min_power) * 2147483647 / (max_power - min_power);

    if (mean_power_norm == 0)
        mean_power_norm = 1;

    for (int i = 0; i < 4; i++) { // number of hr candidates
       HrTopCandidates.power[i] = (HrTopCandidates.power[i] - min_power) * 2147483647 / (max_power - min_power);
       HrTopCandidates.th_cf[i] = HrTopCandidates.power[i] / mean_power_norm;
    }
}


void SignalProcessingAlgorithms::calculate_motion_frequencies(int32_t *re_1, int32_t *im_1, 
                                                            int32_t *re_2, int32_t *im_2, 
                                                            int32_t *re_3, int32_t *im_3) {

    int64_t sum_power[55] = {0};
    int64_t max_power = -9223372036854775808;
    int64_t min_power = 9223372036854775808;

    for (int i = 14; i <= 68; i++) {
        int64_t power_x = (((int64_t)(re_1[i] * re_1[i]) + (int64_t)(im_1[i] * im_1[i])) + 1) >> 1;

        int64_t power_y = (((int64_t)(re_2[i] * re_2[i]) + (int64_t)(im_2[i] * im_2[i])) + 1) >> 1;

        int64_t power_z = (((int64_t)(re_3[i] * re_3[i]) + (int64_t)(im_3[i] * im_3[i])) + 1 ) >> 1;


        sum_power[i-14] = (power_x + power_y + power_z + 1) >> 1;

        if (sum_power[i-14] > max_power) {
            max_power = sum_power[i-14];
        }
        if (sum_power[i-14] < min_power) {
            min_power = sum_power[i-14];
        }
        
    }

    if (max_power == min_power)
        return;

    for (int i = 0; i < 55; i++) {
        motionHrBand.power[i] = (sum_power[i] - min_power) * 2147483647 / (max_power - min_power);
        motionHrBand.frequency[i] = (i + 14) * 100  * 16384 / 2048;
    }   
}


int SignalProcessingAlgorithms::calculate_hr(int16_t bonus_weight, int16_t penalty_weight, int16_t th_cf, 
                                                    int last_stable_hr, int last_hr, int second_last_hr) {
    for (int c = 0; c < MAX_CANDIDATES; c++) { // calculate candidate score
        int candidate_hr = (HrTopCandidates.frequency[c] * 60 + (1LL << 13)) >> 14;

        int imu_idx = HrTopCandidates.index[c] - 14;
        int32_t imu_val = -2147483647;
        int32_t bonus = 0;

        int start = imu_idx - 2;
        int end = imu_idx + 2;

        if (start < 0) {
            start = 0;
        }
        if (end > 54) {
            end = 54;
        }

        for (int i = start; i <= end; i++) {
            if (motionHrBand.power[i] > imu_val) {
                imu_val = motionHrBand.power[i];
            }
        }

        if (last_stable_hr > 0) {
            int diff = candidate_hr - last_stable_hr;
            int diff_abs = absValueOf(diff);
            if (diff_abs < 15) {
                bonus = (15 - diff_abs) * bonus_weight;
            }
        }

        HrTopCandidates.score[c] = HrTopCandidates.power[c] - (imu_val * penalty_weight) + bonus;
    }

    int winner_idx = 0;

    for (int c = 0; c < MAX_CANDIDATES; c++) {
        int32_t max_score = -2147483647;
        if (HrTopCandidates.score[c] > max_score) {
            max_score = HrTopCandidates.score[c];
            winner_idx = c;
        }
    }

    int32_t th_cf_winner = HrTopCandidates.th_cf[winner_idx];
    int hr_winner = (HrTopCandidates.frequency[winner_idx] * 60 + (1LL << 13)) >> 14;

    bool is_singal_sharp = th_cf_winner > th_cf;
    int hr_jump = absValueOf(hr_winner - last_stable_hr);
    int recovery_jump = absValueOf(hr_winner - last_hr);
    int second_recovery_jump = absValueOf(last_hr - second_last_hr);

    switch (StateMachine.state) {
        case 0: // STABLE
            if (is_singal_sharp && (hr_jump <= MAX_HR_DIFF || last_stable_hr == 0)) {
                last_stable_hr = hr_winner;
                second_last_hr = last_hr;
                last_hr = hr_winner;
                
                return last_stable_hr; 
            } else {
                StateMachine.state = 1; // ALERT
                StateMachine.alertCounter = 0;
                second_last_hr = last_hr;
                last_hr = hr_winner;

                return last_stable_hr;
            }
            break;

        case 1: // ALERT
            if (is_singal_sharp) {
                StateMachine.state = 3; // RECOVERY
                StateMachine.recovery_counter = 0;
                second_last_hr = last_hr;
                last_hr = hr_winner;

                return last_stable_hr;
            } else {
                StateMachine.alertCounter++;
                if (StateMachine.alertCounter >= 5) {
                    StateMachine.state = 2; // UNCERTAIN
                    StateMachine.alertCounter = 0;

                    return 0;
                }

                return last_stable_hr;
            }
            break;

        case 2: // UNCERTAIN
            if (is_singal_sharp) {
                StateMachine.good_windows++;
                if (StateMachine.good_windows >= 3) {
                    StateMachine.state = 1;
                    StateMachine.good_windows = 0;

                    return 0;
                }
                return 0;
            }else {
                StateMachine.good_windows = 0;

                return 0;
            }
            break;

        case 3: // RECOVERY
            if (is_singal_sharp) {
                StateMachine.recovery_counter++;
                if (StateMachine.recovery_counter >= 4) {
                    if (recovery_jump <= MAX_HR_DIFF && second_recovery_jump <= MAX_HR_DIFF) {
                        StateMachine.state = 0; // STABLE
                        last_stable_hr = hr_winner;
                        second_last_hr = last_hr;
                        last_hr = hr_winner;
                        StateMachine.recovery_counter = 0;

                        return last_stable_hr;
                    }
                    StateMachine.recovery_counter = 0;
                }
                second_last_hr = last_hr;
                last_hr = hr_winner;

                return last_stable_hr;
            }else {
                StateMachine.state = 1;
                StateMachine.recovery_counter = 0;

                return 0;
            }
            break;
    }
}