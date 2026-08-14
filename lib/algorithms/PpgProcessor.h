#ifndef PPGPROCESSOR_H
#define PPGPROCESSOR_H

#define BUFFER_SIZE             200
#define HISTORY_SIZE            8

#include <Arduino.h>

extern const uint8_t SpO2Table[184];

// Data types

struct PulseData {
    int32_t acRed[BUFFER_SIZE];
    int32_t acIr[BUFFER_SIZE];
    int32_t motionNoise[BUFFER_SIZE];
    int32_t dcRed;
    int32_t dcIr;
    int head = 0;
};

struct HistoryData {
    int32_t HrHistory[HISTORY_SIZE];
    int32_t SpO2History[HISTORY_SIZE];

    int idx_HR = 0;
    int idx_SpO2 = 0;
    int count_HR = 0;
    int count_SpO2 = 0;
};

// Digital filters

class FilterAlgorithms {
    public:
        FilterAlgorithms(void);
        
        static int32_t lowPassFilter(int32_t sample, int32_t &prevOutput, int shift = 2);
        static int32_t highPassFilter(int32_t sample, int32_t &prevFilterState, int16_t alpha = 31739);
        static int32_t medianFilter(int32_t current, int32_t *buffer);
        static int32_t absValueOf(int32_t x);
};


class OpticalChannel {
public:
    OpticalChannel() { reset(); }

    void reset() {
        hpState = 0;
        lpOut = 0;
        medBuffer[0] = 0;
        medBuffer[1] = 0;
        sum = 0;
    }

    void resetSum() { sum = 0; }

    int32_t process(int32_t rawSample, FilterAlgorithms& filters) {
        int32_t clean = filters.medianFilter(rawSample, medBuffer);
        medBuffer[1] = medBuffer[0]; 
        medBuffer[0] = rawSample;
        sum += clean;

        int32_t ac = filters.highPassFilter(clean, hpState);
        return filters.lowPassFilter(ac, lpOut);
    }

    int64_t getSum() const { return sum; }

private:
    int32_t hpState;
    int32_t lpOut;
    int32_t medBuffer[2];
    int64_t sum;
};

struct AxisFilter {
    int32_t lpOut = 0;

    void reset() { 
        lpOut = 0; 
    }

    int32_t process(int16_t rawSample, FilterAlgorithms& filters) {
        return rawSample - filters.lowPassFilter(rawSample, lpOut);
    }
};

// PulseOximetry processing algorithms              

class SignalProcessingAlgorithms {
    public:
        SignalProcessingAlgorithms(void);

        int32_t calculateSpO2(const PulseData &p, int samplingRate, int &heartRate);

        HistoryData historyData;
};


#endif