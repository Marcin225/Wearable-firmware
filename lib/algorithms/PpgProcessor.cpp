#include "PpgProcessor.h"

// SpO2 table


const uint8_t SpO2Table[184]={ 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99, 
              99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 
              100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97, 
              97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91, 
              90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81, 
              80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 
              66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 
              49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29, 
              28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5, 
              3, 2, 1 } ;


// Filter Algorithms


FilterAlgorithms::FilterAlgorithms(void) { }

int32_t FilterAlgorithms::medianFilter(int32_t current, int32_t *buffer) {
    int32_t v[3] = {current, buffer[0], buffer[1]};

    if (v[0] > v[1]) {int32_t t = v[0]; v[0] = v[1]; v[1] = t;}
    if (v[1] > v[2]) {int32_t t = v[1]; v[1] = v[2]; v[2] = t;}
    if (v[0] > v[1]) {int32_t t = v[0]; v[0] = v[1]; v[1] = t;}

    return v[1];
}

int32_t FilterAlgorithms::lowPassFilter(int32_t sample, int32_t &prevOutput, int shift) {
    // y[n] = y[n-1] + (x[n] - y[n-1]) >> 2
    int32_t output = ((sample - prevOutput) >> shift) + prevOutput; // alfa = 0.25     ( >> 2)
    prevOutput = output;

    return output;
}

int32_t FilterAlgorithms::highPassFilter(int32_t sample, int32_t &prevFilterState, int16_t alpha) {  // alpha = 0.9686    alpha * 2^15 = 31739
    // y[n] = x[n] - x[n-1] + R * y[n-1]
    int32_t filterState = (sample << 15) + ((alpha * prevFilterState) >> 15);
    int32_t output = (filterState - prevFilterState) >> 15;
    prevFilterState = filterState;

    return output;
}

int32_t FilterAlgorithms::absValueOf(int32_t x) {
    return (x < 0) ? -x : x;
}


// Helpers


static int32_t median_result(int32_t *arr, int n) {
    // insertion sort
    for (int i = 1; i < n; i++) {
        int32_t temp = arr[i];
        int j = i - 1;
        while (j >= 0 && temp < arr[j]) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = temp;
    }
    // return median element
    if (n & 1) return arr[n / 2];
    return (arr[n / 2 - 1] + arr[n / 2]) >> 1;
}

static inline int32_t i32_abs(int32_t x) {
    return (x < 0) ? -x : x;
}


// Signal processing algorithms

SignalProcessingAlgorithms::SignalProcessingAlgorithms(void) { }

// ****************** Heart rate calculation using peak detection method ********************

static int estimateHR_peak(int *peaks, int peakCount, int MAX_BEATS, int fs, int &peakConfidenceScore) {
    const int HR_MIN = 40;
    const int HR_MAX = 160;

    // Expected heart rate min/max intervals calculation
    const int32_t I_Min = (fs * 60) / HR_MAX;
    const int32_t I_Max = (fs * 60) / HR_MIN;

    int32_t intervals[MAX_BEATS];
    int ic = 0;
    for (int i = 1; i < peakCount; i++) {
        int32_t d = peaks[i] - peaks[i-1];

        // only keep realistic human limits intervals

        if (d >= I_Min && d <= I_Max && ic < MAX_BEATS) {
            intervals[ic++] = d;
        }
    }

    if (ic == 0) {
        peakConfidenceScore = 0;
        return 0;
    }

    int32_t medianDeviation = 0;
    int32_t Deviation = 0;

    int32_t med = median_result(intervals, ic);

    // calculate a confidence score based on how stable the rhythm is

    if (ic >= 2) {

        int32_t dev[MAX_BEATS];
        for (int i = 0; i < ic; i++) {
            dev[i] = i32_abs(intervals[i] - med); // deviation from the median
        }

        medianDeviation = median_result(dev, ic);

        // more stable rhythm -> better score

        int32_t perfectDeviation = (med * 5) / 100;
        int32_t badDeviation = (med * 30) / 100;

        if (perfectDeviation >= medianDeviation) {
            peakConfidenceScore = 100;
        }else if (badDeviation <= medianDeviation) {
            peakConfidenceScore = 0;
        }else {
            peakConfidenceScore = 100 - (((medianDeviation - perfectDeviation) * 100) / (badDeviation - perfectDeviation));
        }
    }else if (ic == 1) {
        peakConfidenceScore = 35;
    }
    

    int hr = 0;
    if (med > 0) {
        hr = (fs * 60) / med; // calculate output HR from median
    }
    
    return hr;
}

// ****************** Heart rate calculation using autocorrelation method ********************

static int estimateHR_autocorr(const int32_t *sig, int n, int fs, int &autoConfidenceScore) {
    const int HR_MIN = 40;
    const int HR_MAX = 160;

    const int lagMin = (fs * 60) / HR_MAX; // 37.5
    const int lagMax = (fs * 60) / HR_MIN; // 150

    int64_t sumAbs = 0;
    int64_t sumMean = 0;

    // Evaluate basic signal quality

    for (int i = 0; i < n; i++) {
        int32_t a = sig[i] < 0 ? -sig[i] : sig[i];
        if (a > 600) a = 600;

        sumAbs += a;
        sumMean += sig[i];
    }
    int32_t meanAbs = (int32_t)(sumAbs / n);
    int32_t MeanSig = (int32_t)(sumMean / n);

    if (meanAbs < 4) {
        autoConfidenceScore = 0;
        return 0;
    }else if (meanAbs >= 40) {
        autoConfidenceScore = 100;
    }else {
        autoConfidenceScore = ((meanAbs - 4) * 100) / (40 - 4);
    }

    int bestLag = 0;
    int64_t bestScore = 0;
    int64_t secondBestScore = 0;

    // buffer protection

    if ((lagMax - lagMin + 1 > 128) || (lagMax - lagMin + 1 <= 0)) {
        autoConfidenceScore = 0;
        return 0;
    }

    static int64_t scoreTab[128]; // if Lag_max or lag_min change -> need more space

    // calculating autocorrelation

    for (int lag = lagMin; lag <= lagMax; lag++) {
        int64_t score = 0;
        for (int i = 0; i < n - lag; i++) {
            int32_t x1 = sig[i] - MeanSig;
            int32_t x2 = sig[i + lag]  - MeanSig;

            score += (int64_t)x1 * (int64_t)x2;
        }
        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
        scoreTab[lag - lagMin] = score;
    }

    // find the second best match

    for (int lag = lagMin; lag <= lagMax; lag++) {
        if (lag >= bestLag - 15 && lag <= bestLag + 15) {// (fs * 150) / 1000 = 15    150ms
            continue;
        }

        int index = lag - lagMin;
        if (scoreTab[index] > secondBestScore) {
            secondBestScore = scoreTab[index];
        }
    }

    // adjust confidence based on how clear the main peak is

    int32_t scoreRatio = 0;

    if (bestScore > 0) {
        scoreRatio = (int32_t)(((bestScore - secondBestScore) * 100) / bestScore);
    }

    if (scoreRatio >= 40) { // sharp peak -> high confidence
        autoConfidenceScore += 30;
    }else if (scoreRatio >= 30) {
        autoConfidenceScore += 20;
    }else if (scoreRatio >= 15) {
        autoConfidenceScore += 10;
    }else {
        autoConfidenceScore -= 10;
    }

    // harmonic penalty: avoid locking onto a false double beat

    int bestLag_x2 = bestLag * 2;

    if (bestLag_x2 <= lagMax) {
        int idx = bestLag_x2 - lagMin;

        int64_t score2 = scoreTab[idx];

        if (score2 * 100 > bestScore * 80) {
            autoConfidenceScore -= 40;
        }
    }

    if (autoConfidenceScore > 100) autoConfidenceScore = 100;
    if (autoConfidenceScore < 0) autoConfidenceScore = 0;


    if (bestLag == 0)  {
        autoConfidenceScore = 0;
        return 0;
    }

    // calculate output HR

    int hr = (fs * 60) / bestLag;
    if (hr < HR_MIN || hr > HR_MAX) {
        autoConfidenceScore = 0;
        return 0;
    }
    return hr;
}


int32_t SignalProcessingAlgorithms::calculateSpO2(const PulseData &p, int samplingRate, int &heartRate) {
    int confidenceScore = 0;
    int peakHRConfScore = 0;
    int autoHRConfScore = 0;
    static int reject_counter_hr = 0;
    static int reject_counter_spo2 = 0;

    // estimate the ideal peak-to-peak distance for a given HR

    int MIN_PEAK_DIST = (samplingRate * 450) / 1000;

    if (historyData.count_HR == HISTORY_SIZE) {
        int32_t last_HR = median_result(historyData.HrHistory, HISTORY_SIZE);
        if (last_HR > 0) {
            MIN_PEAK_DIST = (((samplingRate * 60) / last_HR) * 60) / 100;
        }
    }
    if (MIN_PEAK_DIST < 30) {
        MIN_PEAK_DIST = 30;
    }
    if (MIN_PEAK_DIST > 150) {
        MIN_PEAK_DIST = 150;
    }

    const int MAX_BEATS = 12;
    heartRate = 0;

    if (p.dcRed <= 0 || p.dcIr <= 0) {
        return 0;
    }

    // calculation threshold and basic signal quality

    long SumAbs = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        int32_t v = i32_abs(p.acIr[i]);
        if (v > 600) {
            v = 600;
        }
        SumAbs += v;

    }

    int32_t meanAbs = SumAbs / BUFFER_SIZE;

    if (meanAbs < 6) {
        return 0;
    }

    int32_t thr = (meanAbs * 3) / 4;   // 0.75 * meanAbs
    if (thr < 8) thr = 8;
    if (thr > 250) thr = 250;

    // finding peaks

    int peaks[MAX_BEATS];
    int peakCount = 0;
    int lastPeakIndex = -MIN_PEAK_DIST;

    for (int i = 1; i < BUFFER_SIZE - 1; i++) {
        int32_t v0 = p.acIr[i];

        if (v0 < -thr && v0 < p.acIr[i - 1] && v0 < p.acIr[i + 1]) { 
            if ((i - lastPeakIndex) >= MIN_PEAK_DIST) {
                peaks[peakCount++] = i;
                lastPeakIndex = i;

                if (peakCount >= MAX_BEATS) {
                    break;
                }
            }
        }
    }

    // Combine heart rate results from two autocorrelation and peak detection

    if (peakCount >= 2) {
        int heartRate_peak = estimateHR_peak(peaks, peakCount, MAX_BEATS, samplingRate, peakHRConfScore);
        int heartRate_auto = estimateHR_autocorr(p.acIr, BUFFER_SIZE, samplingRate, autoHRConfScore);

        if (autoHRConfScore > 65 && peakHRConfScore > 65 && i32_abs(heartRate_peak - heartRate_auto) <= 8) {
            heartRate = (heartRate_peak + heartRate_auto) / 2;
        }else if (autoHRConfScore >= 70 && autoHRConfScore > peakHRConfScore) {
            heartRate = heartRate_auto;
        }else if (peakHRConfScore >= 75 && autoHRConfScore < peakHRConfScore) {
            heartRate = heartRate_peak;
        }else {
            heartRate = 0;
        }

        // ignore garbage signal

        if (heartRate == 0) {
            reject_counter_hr = 0;
        }

        // fast adaptation logic for Heart Rate

        if (heartRate > 0 && historyData.count_HR == HISTORY_SIZE) {
            if (i32_abs(heartRate - median_result(historyData.HrHistory, HISTORY_SIZE)) > 10) {
                reject_counter_hr += 1;

                if (reject_counter_hr < 4) {
                    heartRate = 0;
                }else {
                    historyData.count_HR = 0;
                    historyData.idx_HR = 0;
                    reject_counter_hr = 0;
                }
            }else {
                reject_counter_hr = 0;
            }
        }

        if (heartRate > 0) {
            historyData.HrHistory[historyData.idx_HR] = heartRate;
            historyData.idx_HR = (historyData.idx_HR + 1) & (HISTORY_SIZE - 1); // modulo operation % 8 -> HISTORY_SIZE = 8 -> (idx & 7)

            if (historyData.count_HR < HISTORY_SIZE) {
                historyData.count_HR++;
            }
        }

        
        if (heartRate == 0) {
        return 0;
        }

        // calculate SpO₂ using the ratio-of-ratios method

        int32_t rValues[MAX_BEATS];
        int rCount = 0;

        for (int i = 1; i < peakCount; i++) {
            int currentPeak = peaks[i];
            int prevPeak = peaks[i - 1];

            int minIr = 2147483647, maxIr = -2147483647;
            int minRed = 2147483647, maxRed = -2147483647;

            // find the peak-to-peak amplitude (AC) for specific heartbeat

            for (int j = prevPeak; j <= currentPeak; j++) {
                if (p.acIr[j] > maxIr) maxIr = p.acIr[j];
                if (p.acIr[j] < minIr) minIr = p.acIr[j];

                if (p.acRed[j] > maxRed) maxRed = p.acRed[j];
                if (p.acRed[j] < minRed) minRed = p.acRed[j];
            }

            int32_t acIr = maxIr - minIr;
            int32_t acRed = maxRed - minRed;

            // ignore abnormally amplitudes

            if (acIr < meanAbs / 4) continue;
            if (acIr > meanAbs * 6) continue;

            // calculate ratio of ratios (AC_red / DC_red) / (AC_ir / DC_ir)

            int64_t num = (int64_t)acRed * (int64_t)p.dcIr * 100;
            int64_t den = (int64_t)p.dcRed * (int64_t)acIr;
            if (den == 0) continue;

            int32_t R = (int32_t)(num / den);

            // limit R according to the calibration table

            if (R >= 20 && R < 184) {
                rValues[rCount++] = R;
            }
        }

        if (rCount == 0) {
            return 0;
        }

        int32_t finalR = median_result(rValues, rCount);
        int SpO2 = SpO2Table[finalR];

        // fast adaptation logic for SpO2

        if (SpO2 == 0) {
            reject_counter_spo2 = 0;
        }

        if (SpO2 > 0 && historyData.count_SpO2 == HISTORY_SIZE) {
            if (i32_abs(SpO2 - median_result(historyData.SpO2History, HISTORY_SIZE)) >= 2) {
                reject_counter_spo2++;

                if (reject_counter_spo2 < 4) {
                    SpO2 = 0;
                }else {
                    historyData.count_SpO2 = 0;
                    historyData.idx_SpO2 = 0;
                    reject_counter_spo2 = 0;
                }
            }else {
                reject_counter_spo2 = 0;
            }
        }

        if (SpO2 > 0) {
            historyData.SpO2History[historyData.idx_SpO2] = SpO2;
            historyData.idx_SpO2 = (historyData.idx_SpO2 + 1) & (HISTORY_SIZE - 1); // modulo operation % 8 -> HISTORY_SIZE = 8 -> (idx & 7)

            if (historyData.count_SpO2 < HISTORY_SIZE) {
                historyData.count_SpO2++;
            }
        }

        return SpO2;

    }else {
        return 0;
    }
}