#ifndef ALGORITHM_RFFT_H
#define ALGORITHM_RFFT_H

#include <stdint.h>

class SignalProcessingAlgorithms {
    public:
        SignalProcessingAlgorithms(void);

        void rfft(int32_t *re, int32_t *im, int N); // N -> 1024
    
    private:

        void calculate_angles(int32_t *wr, int32_t * wi, int angle_idx);
        void swap(int32_t *tab1, int32_t *tab2);
};

#endif