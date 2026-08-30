/**
 * C++ test wrapper for HR candidate extraction validation.
 * Generates a CSV with the strongest spectral HR candidates to verify
 * the fixed-point peak detection and sorting against the Python reference.
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

#include "config.h"
#include "pre_processor.h"
#include "signal_channel.h"
#include "post_processor.h"


struct CsvRow {
    double timestamp;
    int32_t irRaw;
    int32_t redRaw;
    int32_t accX;
    int32_t accY;
    int32_t accZ;
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

    static void calculateHrCandidates(SignalProcessingAlgorithms &algorithm, int32_t *re, int32_t *im) {
        algorithm.calculate_hr_candidates(re, im);
    }

    static const hrCandidatesNorm &getHrCandidates(const SignalProcessingAlgorithms &algorithm) {
        return algorithm.HrTopCandidates;
    }
};


bool parseCsvRow(const std::string &line, CsvRow &row) {
    std::stringstream stream(line);
    std::string value;

    try {
        std::getline(stream, value, ',');

        std::getline(stream, value, ',');
        row.irRaw = std::stoi(value);
    }
    catch (const std::exception &) {
        return false;
    }

    return true;
}


int main(int argc, char **argv) {
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

    SignalProcessingTestAccess::processRfft(algorithm, bufferIR.sampleBuffer, bufferIR.re, bufferIR.im, FFT_SIZE);
    SignalProcessingTestAccess::calculateHrCandidates(algorithm, bufferIR.re, bufferIR.im);

    const hrCandidatesNorm &candidates = SignalProcessingTestAccess::getHrCandidates(algorithm);

    outFile << "candidate;index;power;frequency_q14\n";

    for (int i = 0; i < MAX_CANDIDATES; i++) {
        outFile << i << ';'
                << (int)candidates.index[i] << ';'
                << candidates.power[i] << ';'
                << candidates.frequency[i] << '\n';
    }

    return EXIT_SUCCESS;
}