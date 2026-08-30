"""
Reference CSV generator for HR candidate extraction validation:
 - preprocessing: median filtering and bandpass (0.4 - 4.0 Hz)
 - spectral analysis: zero-mean, Hann window, and power spectrum estimation
 - peak detection: min-max normalization, Crest Factor (CF) evaluation, and peak sorting

Processes raw IR sensor data to extract up to 4 most prominent heart rate
frequency candidates. Exports results (including Q14 fixed-point frequencies)
to a reference CSV for C/C++ algorithm verification

Args:
    input (str): Path to the input CSV file containing raw signals
    output (str): Path to the output CSV file for HR candidates
"""

import argparse
import numpy as np
import pandas as pd

from scipy.fft import rfft, rfftfreq
from scipy.signal import butter, sosfilt, sosfilt_zi, windows, find_peaks


BUFFER_SIZE = 1024 # number of time-domain samples processed
FFT_SIZE = 2048 # zero-padded FFT size
FS = 100 # sensor sampling frequency in Hz

LOW_FILTER = 0.4 # bandpass lower cutoff in Hz
HIGH_FILTER = 4.0 # bandpass upper cutoff in Hz
FILTER_ORDER = 2 # butterworth filter order

LOW_HR = 0.67 # minimum operating Hr frequency (40 bpm)
HIGH_HR = 3.3 # maximum operating Hr frequency (200 bpm)

MAX_CANDIDATES = 4 # number of candidates


class MedianFilter3:
    def __init__(self):
        self.buffer = [0, 0]
        self.initialized = False

    def process(self, sample):
        # initialize buffer with the first sample to avoid edge artifacts
        if not self.initialized:
            self.buffer[0] = sample
            self.buffer[1] = sample
            self.initialized = True

        values = [sample, self.buffer[0], self.buffer[1]]
        values.sort()

        median = values[1]

        self.buffer[1] = self.buffer[0]
        self.buffer[0] = sample

        return median


def preprocess_signal(signal):
    median_filter = MedianFilter3()
    median_signal = []

    for sample in signal:
        median_signal.append(median_filter.process(int(sample)))

    median_signal = np.asarray(median_signal, dtype=float)

    # generate filter coefficients in SOS format for numerical stability
    sos = butter(FILTER_ORDER, [LOW_FILTER, HIGH_FILTER], btype="bandpass", fs=FS, output="sos")

    # minimize start-up transients
    zi = sosfilt_zi(sos) * median_signal[0]

    filtered_signal, _ = sosfilt(sos, median_signal, zi=zi)

    return filtered_signal


def process_fft(signal):
    signal = np.asarray(signal, dtype=float)

    signal = signal - np.mean(signal) # remove dc

    window = windows.hann(len(signal)) # Hann window to reduce spectral leakage
    signal = signal * window

    spec = np.abs(rfft(signal, n=FFT_SIZE)) ** 2
    freq = rfftfreq(FFT_SIZE, d=1 / FS)

    return spec, freq


def detect_hr_candidates(clean_ir):
    spec, freq = process_fft(clean_ir)

    mask = (freq >= LOW_HR) & (freq <= HIGH_HR)

    band_indices = np.where(mask)[0]
    freq_range = freq[mask]
    spec_range = spec[mask]

    if len(spec_range) == 0:
        return []

    max_val = np.max(spec_range)
    min_val = np.min(spec_range)

    if max_val <= 1e-9 or max_val == min_val:
        return []

    spec_norm = (spec_range - min_val) / (max_val - min_val)

    # rms of the normalized spectrum is used as the baseline for crest factor (peak/rms)
    rms = np.sqrt(np.mean(spec_norm ** 2))

    if rms <= 1e-12:
        return []

    peaks, _ = find_peaks(spec_norm, distance=2, height=0.12, prominence=0.05)

    if len(peaks) == 0:
        return []

    peak_spec = spec_norm[peaks]
    peak_cf = peak_spec / rms

    order = np.argsort(peak_spec)[::-1]

    candidates = []

    for candidate_idx in order[:MAX_CANDIDATES]:
        peak = peaks[candidate_idx]

        fft_bin = int(band_indices[peak])

        # shift fractional frequency by 14 bits (Q14 format) for better readability (used in C++ algorithm)
        frequency_q14 = fft_bin * FS * 16384 // FFT_SIZE

        candidates.append({
            "index": fft_bin,
            "power": float(peak_spec[candidate_idx]),
            "frequency_q14": frequency_q14,
            "cf": float(peak_cf[candidate_idx])
        })

    return candidates


def main(input_path, output_path):
    df = pd.read_csv(input_path)

    if "ir_raw" not in df.columns:
        raise ValueError("No ir_raw column")

    if len(df) < BUFFER_SIZE:
        raise ValueError(f"Not enough samples: {len(df)}, required: {BUFFER_SIZE}")

    ir_raw = df["ir_raw"].iloc[:BUFFER_SIZE].to_numpy(dtype=np.int32)

    clean_ir = preprocess_signal(ir_raw)

    candidates = detect_hr_candidates(clean_ir)

    rows = []

    for i in range(MAX_CANDIDATES):
        if i < len(candidates):
            rows.append({
                "candidate": i,
                "index": candidates[i]["index"],
                "power": candidates[i]["power"],
                "frequency_q14": candidates[i]["frequency_q14"]
            })
        else:
            rows.append({
                "candidate": i,
                "index": 0,
                "power": 0.0,
                "frequency_q14": 0
            })

    output = pd.DataFrame(rows, columns=["candidate", "index", "power", "frequency_q14"])

    output.to_csv(output_path, sep=";", index=False, float_format="%.8f")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()

    main(args.input, args.output)

