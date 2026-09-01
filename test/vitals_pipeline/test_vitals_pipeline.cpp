/**
 * C++ test wrapper for full SpO2 and HR algorithm pipeline validation.
 * Processes the dataset using a sliding window of 1024 samples shifted by 256.
 *
 * Exports raw HR, smoothed HR, SpO2, reference HR from the Magene belt
 * and intermediate AC/DC values to a CSV file for analysis.
 *
 * Args:
 *     input.csv  : Path to the input CSV containing raw sensor data
 *     output.csv : Path to the output CSV for the computed vitals values
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstring>

#include "config.h"
#include "signal_channel.h"
#include "post_processor.h"


struct CsvRow {
    int32_t irRaw;
    int32_t redRaw;
    int32_t accX;
    int32_t accY;
    int32_t accZ;
    int32_t mageneHr;
};


struct RawBuffer {
    int32_t ir[BUFFER_SIZE];
    int32_t red[BUFFER_SIZE];
};


bool parseCsvRow(const std::string &line, CsvRow &row) {
    std::stringstream stream(line);
    std::string value;

    try {
        std::getline(stream, value, ',');

        std::getline(stream, value, ',');
        row.irRaw = std::stoi(value);

        std::getline(stream, value, ',');
        row.redRaw = std::stoi(value);

        std::getline(stream, value, ',');
        row.accX = std::stoi(value);

        std::getline(stream, value, ',');
        row.accY = std::stoi(value);

        std::getline(stream, value, ',');
        row.accZ = std::stoi(value);

        std::getline(stream, value, ',');
        std::getline(stream, value, ',');
        std::getline(stream, value, ',');

        std::getline(stream, value, ',');
        row.mageneHr = std::stoi(value);
    }
    catch (const std::exception &) {
        return false;
    }

    return true;
}


// calculate the DC component from the raw 1024-sample signal window
int32_t calculateDc(const int32_t *buffer) {
    int64_t sum = 0;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        sum += buffer[i];
    }

    return (int32_t)(sum / BUFFER_SIZE);
}


// calculate the average reference HR over the current processing window
double getReferenceHr(const int32_t *buffer) {
    int64_t sum = 0;
    int count = 0;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (buffer[i] > 0) {
            sum += buffer[i];
            count++;
        }
    }

    if (count == 0) {
        return 0.0;
    }

    return (double)sum / count;
}


int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: vitals_pipeline_test <input.csv> <output.csv>\n";
        return EXIT_FAILURE;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    std::ifstream file(inputPath);
    std::ofstream outFile(outputPath);

    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << inputPath << '\n';
        return EXIT_FAILURE;
    }

    if (!outFile.is_open()) {
        std::cerr << "Cannot open or create file: " << outputPath << '\n';
        return EXIT_FAILURE;
    }

    FilterAlgorithms filter;
    SignalProcessingAlgorithms algorithm;

    ChannelFilter ir;
    ChannelFilter red;
    ChannelFilter accX;
    ChannelFilter accY;
    ChannelFilter accZ;

    initChannel(filter, ir);
    initChannel(filter, red);
    initChannel(filter, accX);
    initChannel(filter, accY);
    initChannel(filter, accZ);

    RawBuffer rawBuffer{};
    int32_t refBuffer[BUFFER_SIZE] = {0};

    std::string line;
    std::getline(file, line);

    int sampleIdx = 0;
    int windowIdx = 0;
    bool firstSample = true;

    outFile << "window;hr;hr_smooth;magene_hr;spo2;dc_ir;dc_red;ac_ir_power;ac_red_power;bin\n";

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        CsvRow row{};

        if (!parseCsvRow(line, row)) {
            std::cerr << "Incorrect line: " << line << '\n';
            continue;
        }

        algorithm.processBuffer.sample_buffer_Ir[sampleIdx] =
            processChannel(filter, ir, row.irRaw, firstSample);

        algorithm.processBuffer.sample_buffer_Red[sampleIdx] =
            processChannel(filter, red, row.redRaw, firstSample);

        algorithm.processBuffer.sample_buffer_AccX[sampleIdx] =
            processChannel(filter, accX, row.accX, firstSample);

        algorithm.processBuffer.sample_buffer_AccY[sampleIdx] =
            processChannel(filter, accY, row.accY, firstSample);

        algorithm.processBuffer.sample_buffer_AccZ[sampleIdx] =
            processChannel(filter, accZ, row.accZ, firstSample);

        rawBuffer.ir[sampleIdx] = row.irRaw;
        rawBuffer.red[sampleIdx] = row.redRaw;
        refBuffer[sampleIdx] = row.mageneHr;

        firstSample = false;
        sampleIdx++;

        if (sampleIdx < BUFFER_SIZE) {
            continue;
        }

        // SpO2 DC values are calculated from the raw PPG signals
        algorithm.spo2Data.dcIr = calculateDc(rawBuffer.ir);
        algorithm.spo2Data.dcRed = calculateDc(rawBuffer.red);

        VitalResult result = algorithm.calculateVitals(BONUS_Q12, MAIN_PENALTY_Q12, TH_CF_Q12);

        int32_t hrSmooth = algorithm.smooth_hr(result.heartRate);
        double mageneHr = getReferenceHr(refBuffer);

        outFile << windowIdx << ';'
                << result.heartRate << ';'
                << hrSmooth << ';'
                << mageneHr << ';'
                << result.spo2 << ';'
                << algorithm.spo2Data.dcIr << ';'
                << algorithm.spo2Data.dcRed << ';'
                << algorithm.spo2Data.power_acIr[0] << ';'
                << algorithm.spo2Data.power_acRed << ';'
                << algorithm.spo2Data.bin << '\n';

        memmove(
            algorithm.processBuffer.sample_buffer_Ir,
            algorithm.processBuffer.sample_buffer_Ir + CHUNK_SIZE,
            (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t)
        );

        memmove(
            algorithm.processBuffer.sample_buffer_Red,
            algorithm.processBuffer.sample_buffer_Red + CHUNK_SIZE,
            (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t)
        );

        memmove(
            algorithm.processBuffer.sample_buffer_AccX,
            algorithm.processBuffer.sample_buffer_AccX + CHUNK_SIZE,
            (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t)
        );

        memmove(
            algorithm.processBuffer.sample_buffer_AccY,
            algorithm.processBuffer.sample_buffer_AccY + CHUNK_SIZE,
            (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t)
        );

        memmove(
            algorithm.processBuffer.sample_buffer_AccZ,
            algorithm.processBuffer.sample_buffer_AccZ + CHUNK_SIZE,
            (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t)
        );

        memmove(rawBuffer.ir, rawBuffer.ir + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(rawBuffer.red, rawBuffer.red + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(refBuffer, refBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        sampleIdx = BUFFER_SIZE - CHUNK_SIZE;
        windowIdx++;
    }

    if (!file.eof()) {
        std::cerr << "Read error\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}