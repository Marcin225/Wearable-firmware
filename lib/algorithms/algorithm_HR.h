#ifndef ALGORITHM_HR_H
#define ALGORITHM_HR_H

#define BUFFER_SIZE             200
#define HISTORY_SIZE            8

#include <Arduino.h>

extern const uint8_t SpO2Table[184];

struct PulseData {
    int32_t acRed[BUFFER_SIZE];
    int32_t acIr[BUFFER_SIZE];
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


class FilterAlgorithms {
    public:
        FilterAlgorithms(void);
        
        static int32_t lowPassFilter(int32_t sample, int32_t &prevOutput, int shift = 2);
        static int32_t highPassFilter(int32_t sample, int32_t &prevFilterState, int16_t alpha = 31739);
        static int32_t medianFilter(int32_t current, int32_t *buffer);
};
              

class SignalProcessingAlgorithms {
    public:
        SignalProcessingAlgorithms(void);

        int32_t calculateSpO2(const PulseData &p, int samplingRate, int &heartRate);

        HistoryData historyData;
};


#endif