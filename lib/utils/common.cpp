#include "common.h"

int32_t absValueOf(int32_t x) {
    return (x < 0) ? -x : x;
}

uint32_t isqrt(uint64_t value) {
    uint64_t root = 0;
    uint64_t bit = (1ULL << 62);

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        }else {
            root >>= 1;
        }
        bit >>= 2;
    }

    return (uint32_t)root;
}