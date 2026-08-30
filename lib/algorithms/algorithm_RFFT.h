#ifndef ALGORITHM_RFFT_H
#define ALGORITHM_RFFT_H

#include <stdint.h>
#include "fft_tables.h"

class rfftAlgorithm {
public:
    rfftAlgorithm(void);

    // calculate an N-point real FFT from N/2 input samples packed into re and im
    void rfft(int32_t *re, int32_t *im, int N);
    // calculate the power of a single FFT bin without computing the full spectrum
    int64_t calculate_single_bin_power(int32_t *data, int bin, int N);

private:
    // get the Q31 real and imaginary parts of the FFT twiddle factor
    void calculate_angles(int32_t *wr, int32_t * wi, int angle_idx);
    void swap(int32_t *tab1, int32_t *tab2);
};

#endif