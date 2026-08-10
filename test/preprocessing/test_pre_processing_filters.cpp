/**
 * C++ test wrapper for signal preprocessing validation
 * generates a CSV output to verify fixed-point algorithm implementation 
 * (3-point median + Butterworth bandpass) against the Python reference
 * 
 * Args:
 *     input.csv  : Path to the input CSV containing raw sensor data
 *     output.csv : Path to the output CSV for filtered signals
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include "pre_processor.h"

struct CsvRow {
    int32_t irRaw;
    int32_t accX;
    int32_t accY;
    int32_t accZ;
};

struct ChannelFilter {
    biquadFilter lowPass{};
    biquadFilter highPass{};
    int32_t medianBuffer[2] = {0};
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
        std::cerr << "Usage: preprocessing_test <input.csv> <output.csv>\n";
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

    ChannelFilter ir;
    ChannelFilter accX;
    ChannelFilter accY;
    ChannelFilter accZ;

    initChannel(filter, ir);
    initChannel(filter, accX);
    initChannel(filter, accY);
    initChannel(filter, accZ);

    outFile << "ir;acc_x;acc_y;acc_z\n";

    std::string line;
    std::getline(file, line);

    bool firstSample = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        CsvRow row{};

        if (!parseCsvRow(line, row)) {
            std::cerr << "Incorrect line: " << line << '\n';
            continue;
        }

        int32_t cleanIR = processChannel(filter, ir, row.irRaw, firstSample);
        int32_t cleanAccX = processChannel(filter, accX, row.accX, firstSample);
        int32_t cleanAccY = processChannel(filter, accY, row.accY, firstSample);
        int32_t cleanAccZ = processChannel(filter, accZ, row.accZ, firstSample);

        firstSample = false;

        outFile << cleanIR << ';' << cleanAccX << ';' << cleanAccY << ';' << cleanAccZ << '\n';
    }

    if (!file.eof()) {
        std::cerr << "Read error\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}