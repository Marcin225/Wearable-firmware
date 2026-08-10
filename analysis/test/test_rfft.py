"""
Reference CSV generator for RFFT validation:
 - preprocessing: 3-point median filter and 2nd-order bandpass (0.4 - 4.0 Hz)
 - windowing: zero-mean and Hann window applied to the time-domain signal
 - spectral analysis: 2048-point real FFT computing Re, Im, and Power

Processes a fixed buffer of raw wearable sensor data and exports the spectral
components to a reference CSV file for C/C++ algorithm verification

Args:
    input (str): Path to the input CSV file containing raw signals
    output (str): Path to the output CSV file for the FFT spectrum
"""

import argparse
import numpy as np
import pandas as pd
from scipy.fft import rfft
from scipy.signal import butter, sosfilt, sosfilt_zi, windows


FS = 100 # sensor sampling frequency in Hz
BUFFER_SIZE = 1024 # number of time-domain samples processed
FFT_SIZE = 2048 # zero-padded FFT size
SPECTRUM_SIZE = 1025 # number of positive frequency bins (N/2 + 1)

LOW = 0.4 # bandpass lower cutoff in Hz
HIGH = 4.0 # bandpass upper cutoff in Hz
ORDER = 2 # butterworth filter order


class MedianFilter3:
    def __init__(self):
        self.buffer = [0, 0]
        self.initialized = False

    def process(self, current):
        current = int(current)

        # initialize buffer with the first sample to avoid edge artifacts
        if not self.initialized:
            self.buffer[0] = current
            self.buffer[1] = current
            self.initialized = True

        values = [current, self.buffer[0], self.buffer[1]]
        values.sort()

        result = values[1]

        self.buffer[1] = self.buffer[0]
        self.buffer[0] = current

        return result


def median_filter(signal):
    median = MedianFilter3()
    result = []

    for sample in signal:
        result.append(median.process(sample))

    return np.asarray(result, dtype=float)


def butterworth_filter(signal):
    signal = np.asarray(signal, dtype=float)

    # generate filter coefficients in SOS format for numerical stability
    sos = butter(ORDER, [LOW, HIGH], btype="bandpass", fs=FS, output="sos")

    # minimize start-up transients
    zi = sosfilt_zi(sos) * signal[0]
    filtered, _ = sosfilt(sos, signal, zi=zi)

    return filtered


def process_channel(signal):
    signal = median_filter(signal)
    signal = butterworth_filter(signal)

    signal = signal - np.mean(signal) # remove DC offset

    # apply Hann window to reduce spectral leakage
    window = windows.hann(len(signal))
    signal = signal * window

    fft_output = rfft(signal, n=FFT_SIZE)

    real = fft_output.real
    imag = fft_output.imag

    power = (real * real + imag * imag) / 2.0

    return real, imag, power


def main(input_path, output_path):
    df = pd.read_csv(input_path, sep=",")

    required_columns = ["ir_raw", "acc_x"]

    for column in required_columns:
        if column not in df.columns:
            raise KeyError(f"Required column missing: {column}")

    if len(df) < BUFFER_SIZE:
        raise ValueError(f"CSV contains {len(df)} samples, but {BUFFER_SIZE} are required")

    df = df.iloc[:BUFFER_SIZE]

    ir_raw = df["ir_raw"].to_numpy(dtype=np.int64)
    acc_x_raw = df["acc_x"].to_numpy(dtype=np.int64)

    ir_re, ir_im, ir_power = process_channel(ir_raw)
    acc_x_re, acc_x_im, acc_x_power = process_channel(acc_x_raw)

    output = pd.DataFrame({
        "bin": np.arange(SPECTRUM_SIZE),
        "ir_re": ir_re[:SPECTRUM_SIZE],
        "ir_im": ir_im[:SPECTRUM_SIZE],
        "ir_power": ir_power[:SPECTRUM_SIZE],
        "acc_x_re": acc_x_re[:SPECTRUM_SIZE],
        "acc_x_im": acc_x_im[:SPECTRUM_SIZE],
        "acc_x_power": acc_x_power[:SPECTRUM_SIZE]
    })

    output.to_csv(output_path, sep=";", index=False, float_format="%.10f")

    print(f"Input: {input_path}")
    print(f"Output: {output_path}")
    print(f"Input samples: {BUFFER_SIZE}")
    print(f"FFT size: {FFT_SIZE}")
    print(f"Output bins: {len(output)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()

    main(args.input, args.output)