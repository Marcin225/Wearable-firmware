/**
 * Unity-based unit tests for the SpO2 calculation.
 *
 * Test cases:
 *  - zero denominator: verifies invalid AC/DC input handling
 *  - ratio outside valid range: verifies invalid ratio rejection
 *  - smooth ratio change: verifies that consecutive SpO2 values
 *    do not change by more than 5 percentage points
 */

#include <unity.h>
#include <cstdint>
#include <cstdlib>

#include "post_processor.h"


constexpr int MAX_SPO2_JUMP = 5;


// Access to internal signal-processing stages used only by tests.
struct SignalProcessingTestAccess {
    static int calculateSpo2(SignalProcessingAlgorithms &algorithm) {
        return algorithm.calculate_spo2();
    }
};


SignalProcessingAlgorithms algorithm;


// Set AC/DC values that produce the requested ratio exactly.
void setSpo2Ratio(int ratio) {
    constexpr int32_t AC_IR = 1000;
    constexpr int32_t DC_IR = 1000;
    constexpr int32_t DC_RED = 1000;

    int32_t acRed = ratio * 10;

    algorithm.spo2Data.power_acIr[0] = (int64_t)AC_IR * AC_IR;
    algorithm.spo2Data.power_acRed = (int64_t)acRed * acRed;

    algorithm.spo2Data.dcIr = DC_IR;
    algorithm.spo2Data.dcRed = DC_RED;
}


void test_spo2_zero_denominator() {
    algorithm.spo2Data = {};

    algorithm.spo2Data.power_acIr[0] = 1000000;
    algorithm.spo2Data.power_acRed = 1000000;
    algorithm.spo2Data.dcIr = 1000;
    algorithm.spo2Data.dcRed = 0;

    int spo2 = SignalProcessingTestAccess::calculateSpo2(algorithm);

    TEST_ASSERT_EQUAL_INT(0, spo2);
}


void test_spo2_invalid_ratio() {
    setSpo2Ratio(19);

    int spo2Low = SignalProcessingTestAccess::calculateSpo2(algorithm);

    setSpo2Ratio(151);

    int spo2High = SignalProcessingTestAccess::calculateSpo2(algorithm);

    TEST_ASSERT_EQUAL_INT(0, spo2Low);
    TEST_ASSERT_EQUAL_INT(0, spo2High);
}


void test_spo2_no_sudden_jumps() {
    setSpo2Ratio(20);

    int previousSpo2 = SignalProcessingTestAccess::calculateSpo2(algorithm);

    TEST_ASSERT_GREATER_THAN(0, previousSpo2);

    for (int ratio = 21; ratio <= 150; ratio++) {
        setSpo2Ratio(ratio);

        int currentSpo2 = SignalProcessingTestAccess::calculateSpo2(algorithm);

        TEST_ASSERT_GREATER_THAN(0, currentSpo2);

        int jump = abs(currentSpo2 - previousSpo2);

        TEST_ASSERT_LESS_OR_EQUAL_INT(MAX_SPO2_JUMP, jump);

        previousSpo2 = currentSpo2;
    }
}


int main() {
    UNITY_BEGIN();

    RUN_TEST(test_spo2_zero_denominator);
    RUN_TEST(test_spo2_invalid_ratio);
    RUN_TEST(test_spo2_no_sudden_jumps);

    return UNITY_END();
}