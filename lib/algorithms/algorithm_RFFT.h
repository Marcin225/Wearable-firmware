#ifndef ALGORITHM_RFFT_H
#define ALGORITHM_RFFT_H

#include <stdint.h>
#include "fft_tables.h"

class rfftAlgorithm {
public:
    rfftAlgorithm(void);

    void rfft(int32_t *re, int32_t *im, int N); // N -> 1024
    int64_t calculate_single_bin_power(int32_t *data, int bin, int N);

private:

    void calculate_angles(int32_t *wr, int32_t * wi, int angle_idx);
    void swap(int32_t *tab1, int32_t *tab2);
};

#endif