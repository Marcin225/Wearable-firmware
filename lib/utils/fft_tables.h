#ifndef FFT_TABLES_H
#define FFT_TABLES_H

#include <stdint.h>

// precomputed LUTs in Q31 format

// quarter-wave sine LUT for 1024-point RFFT twiddle factors 
// (remaining values derived via phase symmetry)
extern const int32_t sin_table_q31[513];

// Hann window LUT to reduce spectral leakage
extern const int32_t hann_table_q31[1025];

// MAX30102 spo2 table
extern const uint8_t spo2_table[184];

#endif