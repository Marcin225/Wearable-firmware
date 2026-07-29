#ifndef FFT_TABLES_H
#define FFT_TABLES_H

#include <stdint.h>

extern const int32_t sin_table_q31[513]; // RFFT size = 1024
extern const int32_t hann_table_q31[1025];

#endif