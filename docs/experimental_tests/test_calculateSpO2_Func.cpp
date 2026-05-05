#include <Arduino.h>
#include <unity.h>
#include "PpgProcessor.h"

static const int SAMPLING_RATE = 100;

static const int32_t AC_Red_MockData[BUFFER_SIZE] = {
    -7, -5, -5, -5, -4, -5, -4, 0, 1, 1,
    6, 12, 12, 13, 12, 12, 13, 14, 17, 16,
    15, 11, 10, 7, 4, 7, 4, 5, 7, 7,
    5, 6, 6, 10, 7, 10, 9, 8, 9, 8,
    9, 6, 7, 8, 7, 7, 4, 5, 5, 6,
    7, 6, 3, 3, 2, 2, 3, 4, 3, 1,
    -2, -7, -13, -21, -21, -23, -29, -33, -37, -34,
    -34, -27, -26, -27, -21, -14, -11, -15, -19, -15,
    -18, -22, -24, -24, -22, -19, -16, -10, -8, -11,
    -10, -9, -14, -16, -21, -20, -19, -15, -12, -8,
    -2, 4, 2, 10, 10, 8, 10, 8, 6, 4,
    4, 5, 7, 8, 12, 9, 8, 7, 6, 7,
    7, 5, 3, 3, 5, 5, 1, 2, 10, 7,
    8, 5, 3, 8, 7, 13, 16, 21, 23, 18,
    23, 18, 12, 7, 0, -5, -5, -5, -5, -8,
    -3, -2, -3, -4, -13, -16, -23, -32, -38, -43,
    -43, -46, -44, -42, -40, -31, -28, -25, -24, -20,
    -19, -19, -23, -21, -18, -15, -18, -14, -14, -16,
    -21, -19, -18, -18, -16, -13, -9, -6, -5, -6,
    -21, -23, -17, -15, -12, -12, -10, -8, -6, 0
};

static const int32_t AC_IR_MockData[BUFFER_SIZE] = {
    -11, -11, -9, -8, -7, -11, -16, -12, -7, 7,
    11, 20, 26, 27, 31, 35, 37, 42, 40, 40,
    42, 33, 26, 24, 20, 17, 13, 18, 19, 18,
    21, 16, 16, 16, 21, 22, 24, 27, 24, 28,
    24, 25, 18, 25, 22, 24, 23, 23, 19, 16,
    13, 14, 13, 14, 19, 17, 14, 10, 7, 2,
    -4, -14, -31, -47, -59, -82, -86, -90, -92, -89,
    -87, -82, -72, -64, -54, -41, -41, -32, -31, -36,
    -39, -50, -64, -59, -55, -46, -36, -27, -23, -18,
    -23, -28, -35, -42, -47, -52, -46, -40, -31, -22,
    -9, 5, 15, 26, 27, 23, 27, 20, 15, 17,
    13, 19, 17, 22, 29, 30, 23, 20, 19, 15,
    13, 15, 16, 12, 14, 10, 8, 12, 12, 16,
    17, 15, 14, 19, 22, 25, 31, 53, 55, 63,
    58, 54, 41, 26, 11, -3, -13, -13, -12, -11,
    -7, -4, -3, -3, -25, -39, -69, -86, -99, -112,
    -113, -117, -114, -109, -98, -98, -82, -65, -58, -51,
    -46, -49, -53, -51, -50, -46, -43, -41, -40, -45,
    -45, -59, -62, -65, -67, -52, -46, -41, -41, -39,
    -41, -40, -41, -32, -25, -12, 2, 14, 27, 27
};

const int32_t DC_IR_MockData = 120000;
const int32_t DC_Red_MockData  = 115000;

void setUp(void) {

}

void tearDown(void) {

}

void test_calculate_spo2_function() {
    SignalProcessingAlgorithms processor;
    PulseData mockData = {};

    mockData.dcRed = DC_Red_MockData;
    mockData.dcIr = DC_IR_MockData;
    
    for(int i = 0; i < BUFFER_SIZE; i++) {
        mockData.acRed[i] = AC_Red_MockData[i];
        mockData.acIr[i] = AC_IR_MockData[i];
    }

    int heartRate = 0;
    int32_t spo2 = processor.calculateSpO2(mockData, SAMPLING_RATE, heartRate);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(50, heartRate);
    TEST_ASSERT_LESS_OR_EQUAL_INT(100, heartRate);

    TEST_ASSERT_GREATER_OR_EQUAL_INT32(95, spo2);
    TEST_ASSERT_LESS_OR_EQUAL_INT32(100, spo2);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_calculate_spo2_function);

    return UNITY_END();
}