"""
Validation script for final HR and SpO2 estimation.

Compares raw and smoothed HR estimates against the ECG reference belt
and checks SpO2 output for invalid values and sudden changes.

HR validation uses MAE, RMSE and a +/- 5 BPM tolerance.
SpO2 validation checks that valid estimates stay within the expected range
and that consecutive values do not change by more than 5 percentage points.

Args:
    input (str): Path to the CSV containing computed HR, SpO2 and reference HR.
"""

import argparse
import sys
import numpy as np
import pandas as pd


# HR pass/fail thresholds
MAX_HR_MAE = 5.0
MAX_HR_RMSE = 7.0
MIN_HR_WITHIN_5 = 0.50

# SpO2 pass/fail thresholds
MIN_SPO2 = 70
MAX_SPO2 = 100
MAX_SPO2_JUMP = 5


def calculate_hr_metrics(estimated, reference):
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


def print_hr_result(name, metrics):
    if metrics is None:
        print(f"{name}: FAIL - no valid HR")
        return False

    mae, rmse, bias, max_error, within_5, valid = metrics

    passed = (
        mae <= MAX_HR_MAE
        and rmse <= MAX_HR_RMSE
        and within_5 >= MIN_HR_WITHIN_5
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


def calculate_spo2_metrics(spo2):
    valid = spo2[spo2 > 0]

    if len(valid) == 0:
        return None

    in_range = (valid >= MIN_SPO2) & (valid <= MAX_SPO2)

    if len(valid) > 1:
        jumps = np.abs(np.diff(valid))
        max_jump = np.max(jumps)
        jump_count = np.sum(jumps > MAX_SPO2_JUMP)
    else:
        max_jump = 0.0
        jump_count = 0

    return {
        "valid": len(valid),
        "min": np.min(valid),
        "max": np.max(valid),
        "mean": np.mean(valid),
        "in_range_ratio": np.mean(in_range),
        "max_jump": max_jump,
        "jump_count": int(jump_count),
    }


def print_spo2_result(metrics):
    if metrics is None:
        print("SpO2: FAIL - no valid SpO2")
        return False

    passed = (
        metrics["in_range_ratio"] == 1.0
        and metrics["jump_count"] == 0
    )

    print(f"SpO2: {'PASS' if passed else 'FAIL'}")
    print(f"  Valid windows: {metrics['valid']}")
    print(f"  Mean:          {metrics['mean']:.2f}%")
    print(f"  Min:           {metrics['min']:.2f}%")
    print(f"  Max:           {metrics['max']:.2f}%")
    print(f"  In range:      {metrics['in_range_ratio'] * 100:.2f}%")
    print(f"  Max jump:      {metrics['max_jump']:.2f}%")
    print(f"  Jumps > 5:     {metrics['jump_count']}")
    print()

    return passed


def main(input_path):
    df = pd.read_csv(input_path, sep=";")

    required_columns = ["hr", "hr_smooth", "magene_hr", "spo2"]

    for column in required_columns:
        if column not in df.columns:
            print(f"FAIL: missing column: {column}")
            return 1

    reference = df["magene_hr"].to_numpy(dtype=float)
    hr = df["hr"].to_numpy(dtype=float)
    hr_smooth = df["hr_smooth"].to_numpy(dtype=float)
    spo2 = df["spo2"].to_numpy(dtype=float)

    print()
    print("VITALS ESTIMATION TEST")
    print()

    raw_hr_passed = print_hr_result(
        "RAW HR",
        calculate_hr_metrics(hr, reference)
    )

    smooth_hr_passed = print_hr_result(
        "SMOOTH HR",
        calculate_hr_metrics(hr_smooth, reference)
    )

    spo2_passed = print_spo2_result(
        calculate_spo2_metrics(spo2)
    )

    if raw_hr_passed and smooth_hr_passed and spo2_passed:
        print("RESULT: PASS")
        return 0

    print("RESULT: FAIL")
    return 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    args = parser.parse_args()

    sys.exit(main(args.input))