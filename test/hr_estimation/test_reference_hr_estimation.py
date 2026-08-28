"""
validation script for final heart rate estimation
compares the computed hr (raw and smoothed) against an ecg reference belt

evaluates algorithmic accuracy using standard fitness metrics (mae, rmse) 
and checks if the estimates stay within a +/- 5 bpm tolerance band

args:
    input (str): path to the csv containing computed hr and reference hr
"""

import argparse
import sys
import numpy as np
import pandas as pd


# pass/fail thresholds for hr accuracy
MAX_MAE = 5.0 # max mean absolute error in bpm
MAX_RMSE = 7.0 # max root mean square error in bpm
MIN_WITHIN_5 = 0.50 # min 50% of windows must be within +/- 5 bpm


def calculate_metrics(estimated, reference):
    valid = (estimated > 0) & (reference > 0)

    estimated = estimated[valid]
    reference = reference[valid]

    if len(estimated) == 0:
        return None

    error = estimated - reference
    abs_error = np.abs(error)

    mae = np.mean(abs_error)
    rmse = np.sqrt(np.mean(error ** 2))
    bias = np.mean(error)
    max_error = np.max(abs_error)

    within_5 = np.mean(abs_error <= 5.0)

    return mae, rmse, bias, max_error, within_5, len(estimated)


def print_result(name, metrics):
    if metrics is None:
        print(f"{name}: FAIL - no valid HR")
        return False

    mae, rmse, bias, max_error, within_5, valid = metrics

    passed = (
        mae <= MAX_MAE
        and rmse <= MAX_RMSE
        and within_5 >= MIN_WITHIN_5
    )

    print(f"{name}: {'PASS' if passed else 'FAIL'}")
    print(f"  Valid windows: {valid}")
    print(f"  MAE:           {mae:.2f} bpm")
    print(f"  RMSE:          {rmse:.2f} bpm")
    print(f"  Bias:          {bias:.2f} bpm")
    print(f"  Max error:     {max_error:.2f} bpm")
    print(f"  Within 5 bpm:  {within_5 * 100:.2f}%")
    print()

    return passed


def main(input_path):
    df = pd.read_csv(input_path, sep=";")

    required_columns = ["hr", "hr_smooth", "magene_hr"]

    for column in required_columns:
        if column not in df.columns:
            print(f"FAIL: missing column: {column}")
            return 1

    reference = df["magene_hr"].to_numpy(dtype=float)

    hr = df["hr"].to_numpy(dtype=float)
    hr_smooth = df["hr_smooth"].to_numpy(dtype=float)

    print()
    print("HR ESTIMATION TEST")
    print()

    raw_passed = print_result("RAW HR", calculate_metrics(hr, reference))
    smooth_passed = print_result("SMOOTH HR", calculate_metrics(hr_smooth, reference))

    if raw_passed and smooth_passed:
        print("RESULT: PASS")
        return 0

    print("RESULT: FAIL")
    return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    args = parser.parse_args()

    sys.exit(main(args.input))