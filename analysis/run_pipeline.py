"""
Command-line interface for executing and evaluating PPG signal processing methods

Loads raw wearable sensor data, applies the specified algorithm in simulated
real-time chunks, and outputs performance metrics, CSV results, and diagnostic plots

Args:
    --input (str): Path to the raw wearable CSV data
    --method (str): Algorithm to test
    --fs (int): Sensor sampling frequency in Hz (default: 100)
    --out (str): Optional path to save the resulting CSV
    --plot (flag): Enables diagnostic plotting for IR and RED channels
    --plot-seconds (int): Number of initial seconds to plot (default: 20)
    --plot-out (str): Optional output directory for plot images
    --ref-col (str): Optional reference HR column
    --ref-lag-sec (float): Optional reference signal time shift in seconds
"""

import os
import numpy as np
import pandas as pd

from methods.common import BUFFER_SIZE
from methods.peak_autocorr_baseline import CurrentPeakAutocorr
from methods.median_butterworth_baseline import MedianButterworthPeakAutocorr
from methods.FFT_FSM_baseline import FFT_FSM

from utils.data_loader import load_dataset
from utils.metrics import calculate_metrics, calculate_reference_metrics
from utils.plotting import plot_raw_vs_filtered, plot_red_raw_vs_filtered


METHODS = {
    "current": CurrentPeakAutocorr,
    "median_butterworth": MedianButterworthPeakAutocorr,
    "fft_fsm": FFT_FSM,
}


def get_reference_hr(df, ref_col, start_idx, end_idx):
    if ref_col is None:
        return 0.0

    if ref_col not in df.columns:
        return 0.0

    ref_values = df.iloc[start_idx:end_idx][ref_col].dropna()
    ref_values = ref_values[ref_values > 0]

    if len(ref_values) == 0:
        return 0.0

    return float(ref_values.median())


def get_analysis_window_bounds(start, chunk_size, method, df_len):
    end = start + chunk_size

    analysis_window = getattr(method, "buffer", chunk_size)

    if analysis_window is None or analysis_window <= 0:
        analysis_window = chunk_size

    analysis_window = int(analysis_window)

    window_start = max(0, end - analysis_window)
    window_end = min(df_len, end)

    return window_start, window_end


def add_reference_fields(row, output, ref_hr):
    hr = float(output.get("hr", 0.0))
    raw_hr = float(output.get("raw_hr", 0.0))
    last_stable_hr = float(output.get("last_stable_hr", 0.0))

    row["ref_hr"] = float(ref_hr)

    if ref_hr > 0 and hr > 0:
        error = hr - ref_hr
        abs_error = abs(error)

        row["error"] = float(error)
        row["abs_error"] = float(abs_error)
        row["within_5bpm"] = int(abs_error <= 5.0)
        row["within_10pct_or_5bpm"] = int(abs_error <= max(5.0, 0.10 * ref_hr))
    else:
        row["error"] = np.nan
        row["abs_error"] = np.nan
        row["within_5bpm"] = 0
        row["within_10pct_or_5bpm"] = 0

    if ref_hr > 0 and raw_hr > 0:
        row["raw_error"] = float(raw_hr - ref_hr)
        row["raw_abs_error"] = float(abs(raw_hr - ref_hr))
    else:
        row["raw_error"] = np.nan
        row["raw_abs_error"] = np.nan

    if ref_hr > 0 and last_stable_hr > 0:
        row["stable_error"] = float(last_stable_hr - ref_hr)
        row["stable_abs_error"] = float(abs(last_stable_hr - ref_hr))
    else:
        row["stable_error"] = np.nan
        row["stable_abs_error"] = np.nan

    return row


def run_pipeline(
    csv_path,
    method_name,
    fs=100,
    collect_debug=False,
    plot_seconds=20,
    ref_col=None,
    ref_lag_sec=0.0,
):
    if method_name not in METHODS:
        raise ValueError(
            f"Unknown method: {method_name}. Available: {list(METHODS.keys())}"
        )

    df = load_dataset(csv_path)

    method = METHODS[method_name](fs=fs)
    dataset_name = os.path.basename(csv_path)

    chunk_size = getattr(method, "chunk_size", BUFFER_SIZE)

    results = []
    debug_rows = []

    max_debug_samples = int(plot_seconds * fs)
    ref_lag_samples = int(ref_lag_sec * fs)

    for start in range(0, len(df) - chunk_size + 1, chunk_size):
        chunk = df.iloc[start:start + chunk_size]

        output = method.process_chunk(chunk)

        if output is None:
            continue

        window_start, window_end = get_analysis_window_bounds(
            start=start,
            chunk_size=chunk_size,
            method=method,
            df_len=len(df),
        )

        ref_start = max(0, window_start + ref_lag_samples)
        ref_end = min(len(df), window_end + ref_lag_samples)

        ref_hr = get_reference_hr(
            df=df,
            ref_col=ref_col,
            start_idx=ref_start,
            end_idx=ref_end,
        )

        window_center_idx = (window_start + window_end) / 2.0

        row = {
            "dataset": dataset_name,
            "method": method.name,

            "time_s": window_center_idx / fs,

            "window_start_s": window_start / fs,
            "window_end_s": window_end / fs,

            "output_time_s": (start + chunk_size) / fs,

            "hr": output["hr"],
            "peak_conf": output["peak_conf"],
            "auto_conf": output["auto_conf"],

            "state": output.get("state", ""),
            "raw_hr": output.get("raw_hr", 0),
            "last_stable_hr": output.get("last_stable_hr", 0),
            "score": output.get("score", 0),
            "motion_score": output.get("motion_score", 0),
        }

        for idx in [1, 2, 3]:
            row[f"top{idx}_hr"] = output.get(f"top{idx}_hr", 0)
            row[f"top{idx}_score"] = output.get(f"top{idx}_score", 0)
            row[f"top{idx}_ppg"] = output.get(f"top{idx}_ppg", 0)
            row[f"top{idx}_motion"] = output.get(f"top{idx}_motion", 0)
            row[f"top{idx}_cf"] = output.get(f"top{idx}_cf", 0)
            row[f"top{idx}_bonus"] = output.get(f"top{idx}_bonus", 0)

        row = add_reference_fields(
            row=row,
            output=output,
            ref_hr=ref_hr,
        )

        results.append(row)

        if collect_debug and "debug" in output and len(debug_rows) < max_debug_samples:
            dbg = output["debug"]

            n_debug = min(
                len(dbg["ir_raw"]),
                len(dbg["ir_filtered"]),
                len(dbg["red_raw"]),
                len(dbg["red_filtered"]),
            )

            for i in range(n_debug):
                if len(debug_rows) >= max_debug_samples:
                    break

                debug_rows.append({
                    "time_s": (start + i) / fs,
                    "ir_raw": dbg["ir_raw"][i],
                    "ir_filtered": dbg["ir_filtered"][i],
                    "red_raw": dbg["red_raw"][i],
                    "red_filtered": dbg["red_filtered"][i],
                })

    result_df = pd.DataFrame(results)
    debug_df = pd.DataFrame(debug_rows)

    return result_df, debug_df


def print_results_preview(result):
    main_cols = [
        "time_s",
        "output_time_s",
        "hr",
        "ref_hr",
        "abs_error",
        "state",
        "raw_hr",
        "last_stable_hr",
        "peak_conf",
        "score",
        "motion_score",
    ]

    existing_cols = [col for col in main_cols if col in result.columns]

    print("\n=== RESULTS PREVIEW ===")

    if len(result) == 0:
        print("No results.")
        return

    print(result[existing_cols].round(3))


def print_basic_metrics(metrics):
    print("\n=== BASIC METRICS ===")

    main_basic_metrics = [
        "valid_ratio",
        "median_hr",
        "min_hr",
        "max_hr",
        "hr_mad",
        "max_jump",
        "spike_count_15bpm",
        "median_local_deviation",
    ]

    for key in main_basic_metrics:
        if key in metrics:
            value = metrics[key]

            if isinstance(value, float):
                print(f"{key}: {value:.4f}")
            else:
                print(f"{key}: {value}")


def print_reference_metrics(ref_metrics, ref_col):
    if len(ref_metrics) == 0:
        return

    print("\n=== REFERENCE METRICS ===")
    print("REFERENCE COLUMN:", ref_col)

    main_ref_metrics = [
        "ref_windows",
        "valid_ref_windows",
        "ref_valid_ratio",
        "mae",
        "median_abs_error",
        "rmse",
        "bias",
        "max_abs_error",
        "within_5bpm_ratio",
        "within_10pct_or_5bpm_ratio",
        "effective_accurate_ratio",
    ]

    for key in main_ref_metrics:
        if key in ref_metrics:
            value = ref_metrics[key]

            if isinstance(value, float):
                print(f"{key}: {value:.4f}")
            else:
                print(f"{key}: {value}")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()

    parser.add_argument("--input", required=True)
    parser.add_argument("--method", required=True, choices=METHODS.keys())
    parser.add_argument("--fs", type=int, default=100)
    parser.add_argument("--out", default=None)

    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--plot-seconds", type=int, default=20)
    parser.add_argument("--plot-out", default=None)

    parser.add_argument("--ref-col", default=None)
    parser.add_argument("--ref-lag-sec", type=float, default=0.0)

    args = parser.parse_args()

    result, debug = run_pipeline(
        csv_path=args.input,
        method_name=args.method,
        fs=args.fs,
        collect_debug=args.plot,
        plot_seconds=args.plot_seconds,
        ref_col=args.ref_col,
        ref_lag_sec=args.ref_lag_sec,
    )

    metrics = calculate_metrics(result)
    ref_metrics = calculate_reference_metrics(result)

    pd.set_option("display.max_columns", None)
    pd.set_option("display.width", 180)
    pd.set_option("display.max_colwidth", None)

    print("\nDATASET:", os.path.basename(args.input))
    print("METHOD:", args.method)

    print_results_preview(result)
    print_basic_metrics(metrics)
    print_reference_metrics(ref_metrics, args.ref_col)

    if args.out:
        out_dir = os.path.dirname(args.out)

        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        result.to_csv(args.out, index=False)
        print(f"\nSaved results: {args.out}")

    if args.plot and len(debug) > 0:
        dataset_name = os.path.basename(args.input)

        if args.plot_out:
            os.makedirs(args.plot_out, exist_ok=True)

            base_name = os.path.splitext(dataset_name)[0]
            ir_out = os.path.join(args.plot_out, f"{base_name}_{args.method}_ir.png")
            red_out = os.path.join(args.plot_out, f"{base_name}_{args.method}_red.png")
        else:
            ir_out = None
            red_out = None

        plot_raw_vs_filtered(
            debug,
            title=f"{dataset_name} | {args.method} | IR raw vs filtered",
            out_path=ir_out,
        )

        plot_red_raw_vs_filtered(
            debug,
            title=f"{dataset_name} | {args.method} | RED raw vs filtered",
            out_path=red_out,
        )

        if ir_out:
            print(f"Saved IR plot: {ir_out}")

        if red_out:
            print(f"Saved RED plot: {red_out}")

    elif args.plot:
        print("No debug data available for plotting.")