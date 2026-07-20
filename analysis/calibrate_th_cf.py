"""
Parameter sweep script for tuning FFT_FSM:
 - CF threshold: minimum spectral peak quality required to accept an HR estimate

Tests multiple TH_CF values, compares FFT_FSM heart-rate estimates with the
reference HR column, and prints accuracy metrics for each threshold

Args:
    --input (str): Path to the raw wearable CSV data
"""

import argparse
import numpy as np
import pandas as pd

from methods.FFT_FSM_baseline import FFT_FSM
from utils.data_loader import load_dataset


FS = 100
REF_COL = "magene_hr"


def get_ref_hr(df, start, end):
    ref = df.iloc[start:end][REF_COL].dropna()
    ref = ref[ref > 0]

    if len(ref) == 0:
        return 0.0

    return float(ref.median())


def evaluate_one_th_cf(csv_path, th_cf):
    df = load_dataset(csv_path)

    if REF_COL not in df.columns:
        raise ValueError(f"No reference column: {REF_COL}")

    method = FFT_FSM(fs=FS)
    method.TH_CF = th_cf

    chunk_size = method.chunk_size
    buffer_size = method.buffer

    rows = []

    for start in range(0, len(df) - chunk_size + 1, chunk_size):
        chunk = df.iloc[start:start + chunk_size]
        output = method.process_chunk(chunk)

        if output is None:
            continue

        end = start + chunk_size

        ref_start = max(0, end - buffer_size)
        ref_end = end

        ref_hr = get_ref_hr(df, ref_start, ref_end)

        if ref_hr <= 0:
            continue

        hr = float(output["hr"])

        rows.append({
            "hr": hr,
            "ref_hr": ref_hr,
        })

    data = pd.DataFrame(rows)

    if len(data) == 0:
        return {
            "TH_CF": th_cf,
            "ref_windows": 0,
            "valid_windows": 0,
            "valid_ratio": 0.0,
            "MAE": np.nan,
            "RMSE": np.nan,
            "BIAS": np.nan,
            "MAX_ERROR": np.nan,
            "WITHIN_5": 0.0,
            "WITHIN_10PCT_OR_5": 0.0,
            "EFFECTIVE_5": 0.0,
            "EFFECTIVE_10PCT_OR_5": 0.0,
        }

    valid = data[data["hr"] > 0].copy()

    if len(valid) == 0:
        return {
            "TH_CF": th_cf,
            "ref_windows": len(data),
            "valid_windows": 0,
            "valid_ratio": 0.0,
            "MAE": np.nan,
            "RMSE": np.nan,
            "BIAS": np.nan,
            "MAX_ERROR": np.nan,
            "WITHIN_5": 0.0,
            "WITHIN_10PCT_OR_5": 0.0,
            "EFFECTIVE_5": 0.0,
            "EFFECTIVE_10PCT_OR_5": 0.0,
        }

    error = valid["hr"] - valid["ref_hr"]
    abs_error = error.abs()

    threshold_10pct_or_5 = np.maximum(5.0, 0.10 * valid["ref_hr"])

    within_5 = abs_error <= 5.0
    within_10pct_or_5 = abs_error <= threshold_10pct_or_5

    return {
        "TH_CF": th_cf,
        "ref_windows": len(data),
        "valid_windows": len(valid),
        "valid_ratio": len(valid) / len(data),

        "MAE": abs_error.mean(),
        "RMSE": np.sqrt(np.mean(error ** 2)),
        "BIAS": error.mean(),
        "MAX_ERROR": abs_error.max(),

        "WITHIN_5": within_5.mean(),
        "WITHIN_10PCT_OR_5": within_10pct_or_5.mean(),

        "EFFECTIVE_5": within_5.sum() / len(data),
        "EFFECTIVE_10PCT_OR_5": within_10pct_or_5.sum() / len(data),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    args = parser.parse_args()

    results = []

    for th_cf in np.arange(1.5, 6.01, 0.1):
        result = evaluate_one_th_cf(args.input, float(th_cf))
        results.append(result)

    summary = pd.DataFrame(results)

    pd.set_option("display.max_rows", None)
    pd.set_option("display.max_columns", None)
    pd.set_option("display.width", 220)

    print("\n=== TH_CF SWEEP CURRENT FFT_FSM ===")
    print(summary.round(4))

if __name__ == "__main__":
    main()