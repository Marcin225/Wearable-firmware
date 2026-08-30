/**
 * C++ test wrapper for fixed-point RFFT algorithm validation.
 * Generates a spectrum CSV (Re, Im, Power) to verify the fixed-point
 * FFT implementation against the reference model.
 *
 * Args:
 *     input.csv  : Path to the input CSV containing raw sensor data
 *     output.csv : Path to the output CSV for the computed spectrum
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>

#include "config.h"
#include "signal_channel.h"
#include "post_processor.h"


struct CsvRow {
    int32_t irRaw;
    int32_t accX;
};


struct RfftBuffer {
    int32_t sampleBuffer[BUFFER_SIZE];
    int32_t re[SPECTRUM_SIZE];
    int32_t im[SPECTRUM_SIZE];
};


// access to internal signal-processing stages used only by tests
struct SignalProcessingTestAccess {
    static void processRfft(SignalProcessingAlgorithms &algorithm, int32_t *signal, int32_t *re, int32_t *im, int N) {
        algorithm.process_rfft(signal, re, im, N);
    }
};


bool parseCsvRow(const std::string &line, CsvRow &row) {
    std::stringstream stream(line);
    std::string value;

    try {
        std::getline(stream, value, ',');

        std::getline(stream, value, ',');
        row.irRaw = std::stoi(value);

        std::getline(stream, value, ',');

        std::getline(stream, value, ',');
        row.accX = std::stoi(value);
    }
    catch (const std::exception &) {
        return false;
    }

    return true;
}


int64_t calculatePower(int32_t re, int32_t im) {
    return (((int64_t)re * re + (int64_t)im * im) + 1) >> 1;
}


int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: rfft_test <input.csv> <output.csv>\n";
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

    initChannel(filter, ir);
    initChannel(filter, accX);

    RfftBuffer bufferIR{};
    RfftBuffer bufferAccX{};

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
        bufferAccX.sampleBuffer[sampleIdx] = processChannel(filter, accX, row.accX, firstSample);

        firstSample = false;
        sampleIdx++;
    }

    if (sampleIdx < BUFFER_SIZE) {
        std::cerr << "Not enough samples. Read: " << sampleIdx << ", required: " << BUFFER_SIZE << '\n';
        return EXIT_FAILURE;
    }

    SignalProcessingTestAccess::processRfft(algorithm, bufferIR.sampleBuffer, bufferIR.re, bufferIR.im, FFT_SIZE);
    SignalProcessingTestAccess::processRfft(algorithm, bufferAccX.sampleBuffer, bufferAccX.re, bufferAccX.im, FFT_SIZE);

    outFile << "bin;ir_re;ir_im;ir_power;acc_x_re;acc_x_im;acc_x_power\n";

    for (int k = 0; k < SPECTRUM_SIZE; k++) {
        int64_t irPower = calculatePower(bufferIR.re[k], bufferIR.im[k]);
        int64_t accXPower = calculatePower(bufferAccX.re[k], bufferAccX.im[k]);

        outFile << k << ';'
                << bufferIR.re[k] << ';'
                << bufferIR.im[k] << ';'
                << irPower << ';'
                << bufferAccX.re[k] << ';'
                << bufferAccX.im[k] << ';'
                << accXPower << '\n';
    }

    return EXIT_SUCCESS;
}