/**
 * C++ test wrapper for HR candidate extraction validation
 * generates a CSV of the top spectral peaks to verify the fixed-point 
 * peak detection and sorting algorithm against the Python reference model
 * 
 * Args:
 *     input.csv  : Path to the input CSV containing raw sensor data
 *     output.csv : Path to the output CSV for the extracted HR candidates
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include "pre_processor.h"
#include "post_processor.h"

#define BUFFER_SIZE 1024
#define FFT_SIZE 2048
#define SPECTRUM_SIZE 1025

struct CsvRow {
    double timestamp;
    int32_t irRaw;
    int32_t redRaw;
    int32_t accX;
    int32_t accY;
    int32_t accZ;
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

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: hr_candidates_test <input.csv> <output.csv>\n";
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
    RfftBuffer bufferIR{};

    initChannel(filter, ir);

    std::string line;
    std::getline(file, line);

    int sampleIdx = 0;
    bool firstSample = true;

    while (sampleIdx < BUFFER_SIZE && std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        CsvRow row{};

        if (!parseCsvRow(line, row)) {
            std::cerr << "Incorrect line: " << line << '\n';
            continue;
        }

        bufferIR.sampleBuffer[sampleIdx] = processChannel(filter, ir, row.irRaw, firstSample);

        firstSample = false;
        sampleIdx++;
    }

    if (sampleIdx < BUFFER_SIZE) {
        std::cerr << "Not enough samples. Read: " << sampleIdx << ", required: " << BUFFER_SIZE << '\n';
        return EXIT_FAILURE;
    }

    algorithm.process_rfft(bufferIR.sampleBuffer, bufferIR.re, bufferIR.im, FFT_SIZE);
    algorithm.calculate_hr_candidates(bufferIR.re, bufferIR.im);

    outFile << "candidate;index;power;frequency_q14\n";

    for (int i = 0; i < MAX_CANDIDATES; i++) {
        outFile << i << ';'
                << (int)algorithm.HrTopCandidates.index[i] << ';'
                << algorithm.HrTopCandidates.power[i] << ';'
                << algorithm.HrTopCandidates.frequency[i] << '\n';
    }

    return EXIT_SUCCESS;
}