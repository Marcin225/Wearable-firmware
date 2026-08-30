/**
 * Unity-based unit tests for the motion artifact detection algorithm.
 * Validates the 3-axis accelerometer spectral processing.
 *
 * Test cases:
 *  - all-zero input: verifies baseline stability and frequency mapping
 *  - single peak: validates frequency bin extraction and Q31 normalization
 *  - XYZ complex power: checks combined spectral power across all 3 axes
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include <climits>

#include "config.h"
#include "post_processor.h"

constexpr int MOTION_BINS = 55;
constexpr int HR_BIN_START = 14;


// access to internal signal-processing stages used only by tests
struct SignalProcessingTestAccess {
    static void calculateMotionFrequencies(SignalProcessingAlgorithms &algorithm, 
                                    int32_t *reX, int32_t *imX, 
                                    int32_t *reY, int32_t *imY, 
                                    int32_t *reZ, int32_t *imZ) {
        algorithm.calculate_motion_frequencies(reX, imX, reY, imY, reZ, imZ);
    }

    static const motionNorm &getMotionSpectrum(const SignalProcessingAlgorithms &algorithm) {
        return algorithm.motionHrBand;
    }
};


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

    SignalProcessingTestAccess::calculateMotionFrequencies(algorithm, reX, imX, reY, imY, reZ, imZ);

    const motionNorm &motion = SignalProcessingTestAccess::getMotionSpectrum(algorithm);

    for (int i = 0; i < MOTION_BINS; i++) {
        int32_t expectedFrequency = (i + HR_BIN_START) * SAMPLING_RATE_HZ * 16384 / FFT_SIZE;

        TEST_ASSERT_EQUAL_INT64(0, motion.power[i]);
        TEST_ASSERT_EQUAL_INT32(expectedFrequency, motion.frequency[i]);
    }
}


void test_motion_single_peak() {
    clearSpectra();

    reX[30] = 1000;

    SignalProcessingTestAccess::calculateMotionFrequencies(algorithm, reX, imX, reY, imY, reZ, imZ);

    const motionNorm &motion = SignalProcessingTestAccess::getMotionSpectrum(algorithm);

    int peakIdx = 30 - HR_BIN_START;

    for (int i = 0; i < MOTION_BINS; i++) {
        if (i == peakIdx) {
            TEST_ASSERT_EQUAL_INT64(INT32_MAX, motion.power[i]);
        } else {
            TEST_ASSERT_EQUAL_INT64(0, motion.power[i]);
        }
    }

    TEST_ASSERT_EQUAL_INT32(24000, motion.frequency[peakIdx]);
}


void test_motion_xyz_and_complex_power() {
    clearSpectra();

    reX[20] = 60;
    imX[20] = 80;

    reY[30] = 120;
    imY[30] = 160;

    reZ[40] = 180;
    imZ[40] = 240;

    SignalProcessingTestAccess::calculateMotionFrequencies(algorithm, reX, imX, reY, imY, reZ, imZ);

    const motionNorm &motion = SignalProcessingTestAccess::getMotionSpectrum(algorithm);

    int idx20 = 20 - HR_BIN_START;
    int idx30 = 30 - HR_BIN_START;
    int idx40 = 40 - HR_BIN_START;

    TEST_ASSERT_EQUAL_INT64(238609294, motion.power[idx20]);
    TEST_ASSERT_EQUAL_INT64(954437176, motion.power[idx30]);
    TEST_ASSERT_EQUAL_INT64(INT32_MAX, motion.power[idx40]);

    TEST_ASSERT_EQUAL_INT32(16000, motion.frequency[idx20]);
    TEST_ASSERT_EQUAL_INT32(24000, motion.frequency[idx30]);
    TEST_ASSERT_EQUAL_INT32(32000, motion.frequency[idx40]);
}


int main() {
    UNITY_BEGIN();

    RUN_TEST(test_motion_all_zero);
    RUN_TEST(test_motion_single_peak);
    RUN_TEST(test_motion_xyz_and_complex_power);

    return UNITY_END();
}