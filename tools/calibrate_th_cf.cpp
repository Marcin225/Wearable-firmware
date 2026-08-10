/**
 * parameter sweep script for tuning the crest factor threshold (th_cf)
 * iterates through a predefined range of th_cf values, running the complete
 * hr estimation pipeline (filtering, fft, peak detection, motion cancellation)
 * for each value
 *
 * generates a comprehensive metrics report to determine the optimal balance
 * between accuracy and coverage. measured metrics include:
 * - ref windows
 * - valid windows
 * - valid ratio
 * - mae
 * - rmse
 * - bias
 * - max error
 * - within 5 bpm
 * - within 10% or 5 bpm
 * - effective 5 bpm
 * - effective 10% or 5 bpm
 *
 * args:
 *     input.csv  : path to the raw wearable csv data (including ground truth)
 *     output.csv : path to the output csv containing metrics for each th_cf
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include "pre_processor.h"
#include "post_processor.h"

#define BUFFER_SIZE 1024
#define CHUNK_SIZE 256
#define FFT_SIZE 2048
#define SPECTRUM_SIZE 1025

struct CsvRow {
    double timestamp;
    int32_t irRaw;
    int32_t redRaw;
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

struct HrBuffer {
    std::vector<int> estimatedHr;
    std::vector<double> referenceHr;
};

struct Metrics {
    int refWindows;
    int validWindows;

    double validRatio;

    double mae;
    double rmse;
    double bias;
    double maxError;

    double within5;
    double within10PctOr5;

    double effective5;
    double effective10PctOr5;
};

bool parseCsvRow(const std::string& line, CsvRow& row) {
    std::stringstream stream(line);
    std::string value;

    try {
        std::getline(stream, value, ',');
        row.timestamp = std::stod(value);

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
    catch (const std::exception&) {
        return false;
    }

    return true;
}

bool loadCsv(const std::string& csvPath, std::vector<CsvRow>& rows) {
    std::ifstream file(csvPath);

    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << csvPath << '\n';
        return false;
    }

    std::string line;
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        CsvRow row{};

        if (!parseCsvRow(line, row)) {
            std::cerr << "Incorrect line: " << line << '\n';
            continue;
        }

        rows.push_back(row);
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

double getReferenceHr(const int *buffer) {
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

Metrics calculateMetrics(const HrBuffer& hrBuff) {
    Metrics metrics{};

    int within5Count = 0;
    int within10PctOr5Count = 0;

    double absErrorSum = 0.0;
    double squaredErrorSum = 0.0;
    double errorSum = 0.0;
    double maxError = 0.0;

    size_t count = std::min(hrBuff.estimatedHr.size(), hrBuff.referenceHr.size());

    for (size_t i = 0; i < count; i++) {
        double estimatedHr = hrBuff.estimatedHr[i];
        double referenceHr = hrBuff.referenceHr[i];

        if (referenceHr <= 0.0) {
            continue;
        }

        metrics.refWindows++;

        if (estimatedHr <= 0.0) {
            continue;
        }

        metrics.validWindows++;

        double error = estimatedHr - referenceHr;
        double absError = std::abs(error);

        absErrorSum += absError;
        squaredErrorSum += error * error;
        errorSum += error;

        if (absError > maxError) {
            maxError = absError;
        }

        if (absError <= 5.0) {
            within5Count++;
        }

        double threshold10PctOr5 = std::max(5.0, referenceHr * 0.10);

        if (absError <= threshold10PctOr5) {
            within10PctOr5Count++;
        }
    }

    if (metrics.refWindows > 0) {
        metrics.validRatio = (double)metrics.validWindows / metrics.refWindows;
        metrics.effective5 = (double)within5Count / metrics.refWindows;
        metrics.effective10PctOr5 = (double)within10PctOr5Count / metrics.refWindows;
    }

    if (metrics.validWindows > 0) {
        metrics.mae = absErrorSum / metrics.validWindows;
        metrics.rmse = std::sqrt(squaredErrorSum / metrics.validWindows);
        metrics.bias = errorSum / metrics.validWindows;
        metrics.maxError = maxError;
        metrics.within5 = (double)within5Count / metrics.validWindows;
        metrics.within10PctOr5 = (double)within10PctOr5Count / metrics.validWindows;
    }

    return metrics;
}

Metrics runTest(const std::vector<CsvRow>& rows, int16_t thCfQ12) {
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

    int refBuffer[BUFFER_SIZE] = {0};

    HrBuffer hrBuff;

    int sampleIdx = 0;
    bool firstSample = true;

    for (const CsvRow& row : rows) {
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

        int32_t heartRate = algorithm.calculate_hr(61, 3277, thCfQ12);

        double referenceHr = getReferenceHr(refBuffer);

        hrBuff.estimatedHr.push_back(heartRate);
        hrBuff.referenceHr.push_back(referenceHr);

        memmove(bufferIR.sampleBuffer, bufferIR.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        memmove(bufferAccX.sampleBuffer, bufferAccX.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(bufferAccY.sampleBuffer, bufferAccY.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(bufferAccZ.sampleBuffer, bufferAccZ.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        memmove(refBuffer, refBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int));

        sampleIdx = BUFFER_SIZE - CHUNK_SIZE;
    }

    return calculateMetrics(hrBuff);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: tune_th_cf <input.csv> <output.csv>\n";
        return EXIT_FAILURE;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    std::vector<CsvRow> rows;

    if (!loadCsv(inputPath, rows)) {
        return EXIT_FAILURE;
    }

    if (rows.size() < BUFFER_SIZE) {
        std::cerr << "Not enough samples\n";
        return EXIT_FAILURE;
    }

    std::ofstream outFile(outputPath);

    if (!outFile.is_open()) {
        std::cerr << "Cannot open or create file: " << outputPath << '\n';
        return EXIT_FAILURE;
    }

    outFile << "TH_CF;TH_CF_Q12;REF_WINDOWS;VALID_WINDOWS;VALID_RATIO;MAE;RMSE;BIAS;MAX_ERROR;WITHIN_5;WITHIN_10PCT_OR_5;EFFECTIVE_5;EFFECTIVE_10PCT_OR_5\n";

    std::cout << "TH_CF\tQ12\tVALID\tMAE\tRMSE\tBIAS\tMAX\tWITHIN_5\tWITHIN_10PCT_OR_5\tEFFECTIVE_5\tEFFECTIVE_10PCT_OR_5\n";

    for (int th10 = 30; th10 <= 79; th10++) {
        double thCf = th10 / 10.0;
        int16_t thCfQ12 = (int16_t)((th10 * 4096 + 5) / 10);

        Metrics metrics = runTest(rows, thCfQ12);

        outFile << std::fixed << std::setprecision(4)
                << thCf << ';'
                << thCfQ12 << ';'
                << metrics.refWindows << ';'
                << metrics.validWindows << ';'
                << metrics.validRatio << ';'
                << metrics.mae << ';'
                << metrics.rmse << ';'
                << metrics.bias << ';'
                << metrics.maxError << ';'
                << metrics.within5 << ';'
                << metrics.within10PctOr5 << ';'
                << metrics.effective5 << ';'
                << metrics.effective10PctOr5 << '\n';

        std::cout << std::fixed << std::setprecision(1)
                  << thCf << '\t'
                  << thCfQ12 << '\t'
                  << std::setprecision(2)
                  << metrics.validRatio * 100.0 << "%\t"
                  << metrics.mae << '\t'
                  << metrics.rmse << '\t'
                  << metrics.bias << '\t'
                  << metrics.maxError << '\t'
                  << metrics.within5 * 100.0 << "%\t"
                  << metrics.within10PctOr5 * 100.0 << "%\t"
                  << metrics.effective5 * 100.0 << "%\t"
                  << metrics.effective10PctOr5 * 100.0 << "%\n";
    }

    return EXIT_SUCCESS;
}