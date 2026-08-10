"""
Reference CSV generator for preprocessing validation:
 - median filter: 3-point median filter to remove transient spikes and noise
 - butterworth filter: 2nd-order bandpass filter (0.4 - 4.0 Hz) to isolate physiological frequencies

Processes raw wearable sensor data and exports the filtered signals to a
reference CSV file for C/C++ algorithm verification

Args:
    input (str): Path to the input CSV file containing raw signals
    output (str): Path to the output CSV file for filtered signals
"""

import argparse
import numpy as np
import pandas as pd
from scipy.signal import butter, sosfilt, sosfilt_zi


FS = 100 # sensor sampling frequency in Hz
LOW = 0.4 # bandpass lower cutoff in Hz
HIGH = 4.0 # bandpass upper cutoff in Hz
ORDER = 2 # Butterworth filter order


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


def apply_median_filter(signal):
    median = MedianFilter3()
    result = []

    for sample in signal:
        result.append(median.process(sample))

    return np.asarray(result, dtype=float)


def apply_butterworth_filter(signal):
    signal = np.asarray(signal, dtype=float)

    # generate filter coefficients in SOS format for numerical stability
    sos = butter(ORDER, [LOW, HIGH], btype="bandpass", fs=FS, output="sos")

    # minimize start-up transients
    zi = sosfilt_zi(sos) * signal[0]
    filtered, _ = sosfilt(sos, signal, zi=zi)

    return filtered


def process_channel(raw_signal):
    median_signal = apply_median_filter(raw_signal)
    filtered_signal = apply_butterworth_filter(median_signal)

    return filtered_signal


def main(input_path, output_path):
    input_df = pd.read_csv(input_path, sep=",")

    required_columns = ["ir_raw", "acc_x", "acc_y", "acc_z"]

    for column in required_columns:
        if column not in input_df.columns:
            raise KeyError(f"Required column missing: {column}")

    ir_raw = input_df["ir_raw"].to_numpy(dtype=np.int64)

    acc_x_raw = input_df["acc_x"].to_numpy(dtype=np.int64)
    acc_y_raw = input_df["acc_y"].to_numpy(dtype=np.int64)
    acc_z_raw = input_df["acc_z"].to_numpy(dtype=np.int64)

    ir_filtered = process_channel(ir_raw)

    acc_x_filtered = process_channel(acc_x_raw)
    acc_y_filtered = process_channel(acc_y_raw)
    acc_z_filtered = process_channel(acc_z_raw)

    output_df = pd.DataFrame({
        "ir": ir_filtered,
        "acc_x": acc_x_filtered,
        "acc_y": acc_y_filtered,
        "acc_z": acc_z_filtered
    })

    output_df.to_csv(output_path, sep=";", index=False, float_format="%.6f")

    print(f"Input: {input_path}")
    print(f"Output: {output_path}")
    print(f"Samples: {len(output_df)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Input CSV file")
    parser.add_argument("output", help="Output CSV file")
    args = parser.parse_args()

    main(args.input, args.output)