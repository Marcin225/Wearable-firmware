/**
* C++ test wrapper for full HR algorithm pipeline validation
 * processes the dataset using a sliding window of 1024 samples (shifted by 256) 
 * to compute continuous heart rate estimates
 * 
 * exports the estimated HR, smoothed HR and reference HR from EKG belt to a CSV file for benchmarking 
 * 
 * Args:
 *     input.csv  : Path to the input CSV containing raw sensor data
 *     output.csv : Path to the output CSV for the computed heart rates
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstring>
#include "pre_processor.h"
#include "post_processor.h"

#define BUFFER_SIZE 1024
#define CHUNK_SIZE 256
#define FFT_SIZE 2048
#define SPECTRUM_SIZE 1025

constexpr int16_t BONUS_Q12 = 61;
constexpr int16_t PENALTY_Q12 = 3277;
constexpr int16_t TH_CF_Q12 = 13926;

struct CsvRow {
    int32_t irRaw;
    int32_t accX;
    int32_t accY;
    int32_t accZ;
    int32_t mageneHr;
};

struct ChannelFilter {
    biquadFilter lowPass{};
    biquadFilter highPass{};
    int32_t medianBuffer[2] = {0};
};

struct RfftBuffer {
    int32_t sampleBuffer[BUFFER_SIZE];
    int32_t re[SPECTRUM_SIZE];
    int32_t im[SPECTRUM_SIZE];
};

bool parseCsvRow(const std::string& line, CsvRow& row) {
    std::stringstream stream(line);
    std::string value;

    try {
        std::getline(stream, value, ',');

        std::getline(stream, value, ',');
        row.irRaw = std::stoi(value);

        std::getline(stream, value, ',');

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
    catch (const std::exception&) {
        return false;
    }

    return true;
}

void initChannel(FilterAlgorithms& filter, ChannelFilter& channel) {
    filter.initFilter(&channel.lowPass, 11803882, 23607764, 11803882, -1830343161, 806667139);
    filter.initFilter(&channel.highPass, 1073741824, -2147483648, 1073741824, -2110933440, 1037980441);
}

int32_t processChannel(FilterAlgorithms& filter, ChannelFilter& channel, int32_t sample, bool firstSample) {
    if (firstSample) {
        channel.medianBuffer[0] = sample;
        channel.medianBuffer[1] = sample;
    }

    int32_t medianSample = filter.medianFilter(sample, channel.medianBuffer);

    if (firstSample) {
        filter.initBandPassSteadyState(&channel.lowPass, &channel.highPass, medianSample);
    }

    int32_t filteredSample = filter.bandPassFilter(&channel.lowPass, medianSample);
    filteredSample = filter.bandPassFilter(&channel.highPass, filteredSample);

    return filteredSample;
}

double getReferenceHr(const int32_t* buffer) {
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

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: full_pipeline_test <input.csv> <output.csv>\n";
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
    ChannelFilter accX;
    ChannelFilter accY;
    ChannelFilter accZ;

    initChannel(filter, ir);
    initChannel(filter, accX);
    initChannel(filter, accY);
    initChannel(filter, accZ);

    RfftBuffer bufferIR{};
    RfftBuffer bufferAccX{};
    RfftBuffer bufferAccY{};
    RfftBuffer bufferAccZ{};

    int32_t refBuffer[BUFFER_SIZE] = {0};

    std::string line;
    std::getline(file, line);

    int sampleIdx = 0;
    int windowIdx = 0;
    bool firstSample = true;

    outFile << "window;hr;hr_smooth;magene_hr\n";

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        CsvRow row{};

        if (!parseCsvRow(line, row)) {
            std::cerr << "Incorrect line: " << line << '\n';
            continue;
        }

        bufferIR.sampleBuffer[sampleIdx] = processChannel(filter, ir, row.irRaw, firstSample);

        bufferAccX.sampleBuffer[sampleIdx] = processChannel(filter, accX, row.accX, firstSample);
        bufferAccY.sampleBuffer[sampleIdx] = processChannel(filter, accY, row.accY, firstSample);
        bufferAccZ.sampleBuffer[sampleIdx] = processChannel(filter, accZ, row.accZ, firstSample);

        refBuffer[sampleIdx] = row.mageneHr;

        firstSample = false;
        sampleIdx++;

        if (sampleIdx < BUFFER_SIZE) {
            continue;
        }

        algorithm.process_rfft(bufferIR.sampleBuffer, bufferIR.re, bufferIR.im, FFT_SIZE);
        algorithm.calculate_hr_candidates(bufferIR.re, bufferIR.im);

        algorithm.process_rfft(bufferAccX.sampleBuffer, bufferAccX.re, bufferAccX.im, FFT_SIZE);
        algorithm.process_rfft(bufferAccY.sampleBuffer, bufferAccY.re, bufferAccY.im, FFT_SIZE);
        algorithm.process_rfft(bufferAccZ.sampleBuffer, bufferAccZ.re, bufferAccZ.im, FFT_SIZE);

        algorithm.calculate_motion_frequencies(bufferAccX.re, bufferAccX.im,
                                        bufferAccY.re, bufferAccY.im,
                                        bufferAccZ.re, bufferAccZ.im);

        int32_t heartRate = algorithm.calculate_hr(BONUS_Q12, PENALTY_Q12, TH_CF_Q12);
        int32_t hrSmooth = algorithm.smooth_hr(heartRate);

        double mageneHr = getReferenceHr(refBuffer);

        outFile << windowIdx << ';'
                << heartRate << ';'
                << hrSmooth << ';'
                << mageneHr << '\n';

        memmove(bufferIR.sampleBuffer, bufferIR.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        memmove(bufferAccX.sampleBuffer, bufferAccX.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(bufferAccY.sampleBuffer, bufferAccY.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(bufferAccZ.sampleBuffer, bufferAccZ.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

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