"""
validation script for the preprocessing pipeline
compares the C/C++ implementation output against the Python reference model 
to verify algorithmic equivalence using NRMSE and correlation metrics

Args:
    reference (str): Path to the reference CSV
    test (str): Path to the test CSV
"""

import argparse
import sys
import numpy as np
import pandas as pd


# fixed-point vs float tolerance limits
ABS_TOL = 5.0 # max absolute error
REL_TOL = 0.05 # max relative error (5%)

# pass/fail criteria
MAX_NRMSE = 0.10 # max normalized rmse
MIN_CORRELATION = 0.98 # min signal shape correlation
MIN_WITHIN_RATIO = 0.95 # min samples within tolerance

CHANNELS = ["ir", "acc_x", "acc_y", "acc_z"]


def calculate_metrics(reference, test):
    error = test - reference
    abs_error = np.abs(error)

    mae = np.mean(abs_error)
    rmse = np.sqrt(np.mean(error ** 2))

    reference_rms = np.sqrt(np.mean(reference ** 2))

    if reference_rms > 0:
        nrmse = rmse / reference_rms
    else:
        nrmse = 0.0 if rmse == 0 else np.inf

    if np.std(reference) > 0 and np.std(test) > 0:
        correlation = np.corrcoef(reference, test)[0, 1]
    else:
        correlation = 1.0 if np.allclose(reference, test) else 0.0

    tolerance = ABS_TOL + REL_TOL * np.abs(reference)
    within_ratio = np.mean(abs_error <= tolerance)

    max_error = np.max(abs_error)

    return {
        "mae": mae,
        "rmse": rmse,
        "nrmse": nrmse,
        "correlation": correlation,
        "within_ratio": within_ratio,
        "max_error": max_error
    }


def main(reference_path, test_path):
    reference_df = pd.read_csv(reference_path, sep=";")
    test_df = pd.read_csv(test_path, sep=";")

    if len(reference_df) != len(test_df):
        print(f"FAIL: different sample count: reference={len(reference_df)}, test={len(test_df)}")
        return 1

    for channel in CHANNELS:
        if channel not in reference_df.columns:
            print(f"FAIL: missing column in reference: {channel}")
            return 1

        if channel not in test_df.columns:
            print(f"FAIL: missing column in test: {channel}")
            return 1

    all_passed = True

    print()
    print("PREPROCESSING COMPARISON")
    print()

    for channel in CHANNELS:
        reference = reference_df[channel].to_numpy(dtype=float)
        test = test_df[channel].to_numpy(dtype=float)

        metrics = calculate_metrics(reference, test)

        passed = (
            metrics["nrmse"] <= MAX_NRMSE
            and metrics["correlation"] >= MIN_CORRELATION
            and metrics["within_ratio"] >= MIN_WITHIN_RATIO
        )

        if not passed:
            all_passed = False

        status = "PASS" if passed else "FAIL"

        print(f"{channel}: {status}")
        print(f"  MAE:          {metrics['mae']:.4f}")
        print(f"  RMSE:         {metrics['rmse']:.4f}")
        print(f"  NRMSE:        {metrics['nrmse'] * 100:.2f}%")
        print(f"  Correlation:  {metrics['correlation']:.6f}")
        print(f"  Within tol:   {metrics['within_ratio'] * 100:.2f}%")
        print(f"  Max error:    {metrics['max_error']:.4f}")
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