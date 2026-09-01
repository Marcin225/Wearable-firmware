/**
 * Parameter sweep script for tuning the crest factor threshold (TH_CF).
 * Runs the complete HR estimation pipeline for a range of TH_CF values
 * and compares the results with the Magene reference HR.
 *
 * Generated metrics:
 *  - reference windows
 *  - valid windows
 *  - valid ratio
 *  - MAE
 *  - RMSE
 *  - bias
 *  - max error
 *  - within 5 BPM
 *  - within 10% or 5 BPM
 *  - effective 5 BPM
 *  - effective 10% or 5 BPM
 *
 * Args:
 *     input.csv  : Path to the raw wearable CSV data
 *     output.csv : Path to the output CSV containing metrics for each TH_CF
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


struct HrBuffer {
    std::vector<int32_t> estimatedHr;
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


bool loadCsv(const std::string &csvPath, std::vector<CsvRow> &rows) {
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


Metrics calculateMetrics(const HrBuffer &hrBuff) {
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


Metrics runTest(const std::vector<CsvRow> &rows, int32_t thCfQ12) {
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

    HrBuffer hrBuff;

    int sampleIdx = 0;
    bool firstSample = true;

    for (const CsvRow &row : rows) {
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

        int32_t heartRate = SignalProcessingTestAccess::calculateHr(
            algorithm,
            BONUS_Q12,
            MAIN_PENALTY_Q12,
            thCfQ12
        );
        heartRate = algorithm.smooth_hr(heartRate);

        double referenceHr = getReferenceHr(refBuffer);

        hrBuff.estimatedHr.push_back(heartRate);
        hrBuff.referenceHr.push_back(referenceHr);

        memmove(bufferIR.sampleBuffer, bufferIR.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        memmove(bufferAccX.sampleBuffer, bufferAccX.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(bufferAccY.sampleBuffer, bufferAccY.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));
        memmove(bufferAccZ.sampleBuffer, bufferAccZ.sampleBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        memmove(refBuffer, refBuffer + CHUNK_SIZE, (BUFFER_SIZE - CHUNK_SIZE) * sizeof(int32_t));

        sampleIdx = BUFFER_SIZE - CHUNK_SIZE;
    }

    return calculateMetrics(hrBuff);
}


int main(int argc, char **argv) {
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

        // convert the threshold to Q12
        int32_t thCfQ12 = (th10 * 4096 + 5) / 10;

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