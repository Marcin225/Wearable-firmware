"""
validation script for the rfft pipeline
compares the C/C++ fixed-point spectrum against the python reference

verifies spectral shape (correlation) and exact locations of frequency peaks
raw power values are normalized to account for fixed-point scaling differences

args:
    reference (str): path to the reference csv
    test (str): path to the test csv
"""

import argparse
import sys
import numpy as np
import pandas as pd

# physiological hr frequency bins limits
HR_BIN_START = 14
HR_BIN_END = 68

# pass/fail criteria for spectral shape and peak matching
MIN_FULL_CORRELATION = 0.95 # min correlation for the entire spectrum
MIN_HR_CORRELATION = 0.98 # min correlation within the hr frequency band
MAX_HR_NRMSE = 0.10 # max normalized rmse (after min-max scaling)
MAX_PEAK_BIN_DIFF = 1 # max allowed shift in peak frequency bin

CHANNELS = ["ir", "acc_x"]


def normalize(signal):
    min_value = np.min(signal)
    max_value = np.max(signal)

    if max_value == min_value:
        return np.zeros_like(signal)

    return (signal - min_value) / (max_value - min_value)


def calculate_metrics(reference_power, test_power):
    full_correlation = np.corrcoef(reference_power, test_power)[0, 1]

    reference_hr = reference_power[HR_BIN_START:HR_BIN_END + 1]
    test_hr = test_power[HR_BIN_START:HR_BIN_END + 1]

    hr_correlation = np.corrcoef(reference_hr, test_hr)[0, 1]

    reference_norm = normalize(reference_hr)
    test_norm = normalize(test_hr)

    error = test_norm - reference_norm

    rmse = np.sqrt(np.mean(error ** 2))
    reference_rms = np.sqrt(np.mean(reference_norm ** 2))

    if reference_rms > 0:
        nrmse = rmse / reference_rms
    else:
        nrmse = 0.0 if rmse == 0 else np.inf

    reference_peak_bin = HR_BIN_START + np.argmax(reference_hr)
    test_peak_bin = HR_BIN_START + np.argmax(test_hr)

    peak_bin_diff = abs(reference_peak_bin - test_peak_bin)

    return {
        "full_correlation": full_correlation,
        "hr_correlation": hr_correlation,
        "hr_nrmse": nrmse,
        "reference_peak_bin": reference_peak_bin,
        "test_peak_bin": test_peak_bin,
        "peak_bin_diff": peak_bin_diff
    }


def main(reference_path, test_path):
    reference_df = pd.read_csv(reference_path, sep=";")
    test_df = pd.read_csv(test_path, sep=";")

    if len(reference_df) != len(test_df):
        print(f"FAIL: different bin count: reference={len(reference_df)}, test={len(test_df)}")
        return 1

    required_columns = [
        "bin",
        "ir_re",
        "ir_im",
        "ir_power",
        "acc_x_re",
        "acc_x_im",
        "acc_x_power"
    ]

    for column in required_columns:
        if column not in reference_df.columns:
            print(f"FAIL: missing column in reference: {column}")
            return 1

        if column not in test_df.columns:
            print(f"FAIL: missing column in test: {column}")
            return 1

    if not np.array_equal(reference_df["bin"].to_numpy(), test_df["bin"].to_numpy()):
        print("FAIL: bin indices are different")
        return 1

    all_passed = True

    print()
    print("RFFT COMPARISON")
    print()

    for channel in CHANNELS:
        reference_power = reference_df[f"{channel}_power"].to_numpy(dtype=float)
        test_power = test_df[f"{channel}_power"].to_numpy(dtype=float)

        metrics = calculate_metrics(reference_power, test_power)

        passed = (
            metrics["full_correlation"] >= MIN_FULL_CORRELATION
            and metrics["hr_correlation"] >= MIN_HR_CORRELATION
            and metrics["hr_nrmse"] <= MAX_HR_NRMSE
            and metrics["peak_bin_diff"] <= MAX_PEAK_BIN_DIFF
        )

        if not passed:
            all_passed = False

        status = "PASS" if passed else "FAIL"

        print(f"{channel}: {status}")
        print(f"  Full correlation: {metrics['full_correlation']:.6f}")
        print(f"  HR correlation:   {metrics['hr_correlation']:.6f}")
        print(f"  HR NRMSE:         {metrics['hr_nrmse'] * 100:.2f}%")
        print(f"  Reference peak:   {metrics['reference_peak_bin']}")
        print(f"  Test peak:        {metrics['test_peak_bin']}")
        print(f"  Peak bin diff:    {metrics['peak_bin_diff']}")
        print()

    if all_passed:
        print("RESULT: PASS")
        return 0

    print("RESULT: FAIL")
    return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("reference")
    parser.add_argument("test")
    args = parser.parse_args()

    sys.exit(main(args.reference, args.test))