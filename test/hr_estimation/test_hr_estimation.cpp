/**
 * C++ test wrapper for full HR algorithm pipeline validation.
 * Processes the dataset using a sliding window of 1024 samples shifted by 256
 * to generate continuous heart-rate estimates.
 *
 * Exports the estimated HR, smoothed HR and reference HR from the EKG belt
 * to a CSV file for benchmarking.
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

#include "config.h"
#include "signal_channel.h"
#include "post_processor.h"


struct CsvRow {
    int32_t irRaw;
    int32_t accX;
    int32_t accY;
    int32_t accZ;
    int32_t mageneHr;
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

    static void calculateMotionFrequencies(SignalProcessingAlgorithms &algorithm,
                                           int32_t *re1, int32_t *im1,
                                           int32_t *re2, int32_t *im2,
                                           int32_t *re3, int32_t *im3) {
        algorithm.calculate_motion_frequencies(re1, im1, re2, im2, re3, im3);
    }

    static int calculateHr(SignalProcessingAlgorithms &algorithm, int32_t bonusWeight, int32_t penaltyWeight, int32_t thCf) {
        return algorithm.calculate_hr(bonusWeight, penaltyWeight, thCf);
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

        SignalProcessingTestAccess::processRfft(algorithm, bufferIR.sampleBuffer, bufferIR.re, bufferIR.im, FFT_SIZE);
        SignalProcessingTestAccess::calculateHrCandidates(algorithm, bufferIR.re, bufferIR.im);

        SignalProcessingTestAccess::processRfft(algorithm, bufferAccX.sampleBuffer, bufferAccX.re, bufferAccX.im, FFT_SIZE);
        SignalProcessingTestAccess::processRfft(algorithm, bufferAccY.sampleBuffer, bufferAccY.re, bufferAccY.im, FFT_SIZE);
        SignalProcessingTestAccess::processRfft(algorithm, bufferAccZ.sampleBuffer, bufferAccZ.re, bufferAccZ.im, FFT_SIZE);

        SignalProcessingTestAccess::calculateMotionFrequencies(
            algorithm,
            bufferAccX.re, bufferAccX.im,
            bufferAccY.re, bufferAccY.im,
            bufferAccZ.re, bufferAccZ.im
        );

        int32_t heartRate = SignalProcessingTestAccess::calculateHr(algorithm, BONUS_Q12, MAIN_PENALTY_Q12, TH_CF_Q12);
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