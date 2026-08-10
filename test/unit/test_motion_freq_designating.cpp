/**
 * unity-based unit tests for the motion artifact detection algorithm
 * validates the 3-axis accelerometer spectral processing
 * 
 * test cases covered:
 *  - all-zero input: verifies baseline stability and empty spectrum handling
 *  - single peak: validates frequency bin extraction and Q31 peak power calculation
 *  - XYZ complex power: checks the vector sum of Re/Im components across all 3 axes
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include "post_processor.h"

#define SPECTRUM_SIZE 1025
#define MOTION_BINS 55
#define HR_BIN_START 14
#define Q31_MAX 2147483647

SignalProcessingAlgorithms algorithm;

static int32_t reX[SPECTRUM_SIZE];
static int32_t imX[SPECTRUM_SIZE];

static int32_t reY[SPECTRUM_SIZE];
static int32_t imY[SPECTRUM_SIZE];

static int32_t reZ[SPECTRUM_SIZE];
static int32_t imZ[SPECTRUM_SIZE];

void clearSpectra() {
    std::memset(reX, 0, sizeof(reX));
    std::memset(imX, 0, sizeof(imX));

    std::memset(reY, 0, sizeof(reY));
    std::memset(imY, 0, sizeof(imY));

    std::memset(reZ, 0, sizeof(reZ));
    std::memset(imZ, 0, sizeof(imZ));
}

void test_motion_all_zero() {
    clearSpectra();

    algorithm.calculate_motion_frequencies(reX, imX, reY, imY, reZ, imZ);

    for (int i = 0; i < MOTION_BINS; i++) {
        int32_t expectedFrequency = (i + HR_BIN_START) * 100 * 16384 / 2048;

        TEST_ASSERT_EQUAL_INT64(0, algorithm.motionHrBand.power[i]);
        TEST_ASSERT_EQUAL_INT32(expectedFrequency, algorithm.motionHrBand.frequency[i]);
    }
}

void test_motion_single_peak() {
    clearSpectra();

    reX[30] = 1000;

    algorithm.calculate_motion_frequencies(reX, imX, reY, imY, reZ, imZ);

    int peakIdx = 30 - HR_BIN_START;

    for (int i = 0; i < MOTION_BINS; i++) {
        if (i == peakIdx) {
            TEST_ASSERT_EQUAL_INT64(Q31_MAX, algorithm.motionHrBand.power[i]);
        } else {
            TEST_ASSERT_EQUAL_INT64(0, algorithm.motionHrBand.power[i]);
        }
    }

    TEST_ASSERT_EQUAL_INT32(24000, algorithm.motionHrBand.frequency[peakIdx]);
}

void test_motion_xyz_and_complex_power() {
    clearSpectra();

    reX[20] = 60;
    imX[20] = 80;

    reY[30] = 120;
    imY[30] = 160;

    reZ[40] = 180;
    imZ[40] = 240;

    algorithm.calculate_motion_frequencies(reX, imX, reY, imY, reZ, imZ);

    int idx20 = 20 - HR_BIN_START;
    int idx30 = 30 - HR_BIN_START;
    int idx40 = 40 - HR_BIN_START;

    TEST_ASSERT_EQUAL_INT64(238609294, algorithm.motionHrBand.power[idx20]);
    TEST_ASSERT_EQUAL_INT64(954437176, algorithm.motionHrBand.power[idx30]);
    TEST_ASSERT_EQUAL_INT64(Q31_MAX, algorithm.motionHrBand.power[idx40]);

    TEST_ASSERT_EQUAL_INT32(16000, algorithm.motionHrBand.frequency[idx20]);
    TEST_ASSERT_EQUAL_INT32(24000, algorithm.motionHrBand.frequency[idx30]);
    TEST_ASSERT_EQUAL_INT32(32000, algorithm.motionHrBand.frequency[idx40]);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_motion_all_zero);
    RUN_TEST(test_motion_single_peak);
    RUN_TEST(test_motion_xyz_and_complex_power);

    return UNITY_END();
}