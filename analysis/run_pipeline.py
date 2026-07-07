"""
Command-line interface for executing and evaluating PPG signal processing methods.

Loads raw wearable sensor data, applies the specified algorithm (e.g., current,
median_butterworth) in simulated real-time chunks, and outputs performance metrics,
CSV results, and diagnostic plots.

Args:
    --input (str): Path to the raw wearable CSV data.
    --method (str): Algorithm to test ('current' or 'median_butterworth').
    --fs (int): Sensor sampling frequency in Hz (default: 100).
    --out (str): Optional path to save the resulting CSV.
    --plot (flag): Enables diagnostic plotting for IR and RED channels.
    --plot-seconds (int): Number of initial seconds to plot (default: 20).
    --plot-out (str): Optional directory to save the plot images.
"""

import os
import pandas as pd

from methods.common import BUFFER_SIZE
from methods.peak_autocorr_baseline import CurrentPeakAutocorr
from methods.median_butterworth_baseline import MedianButterworthPeakAutocorr

from utils.data_loader import load_dataset
from utils.metrics import calculate_metrics
from utils.plotting import plot_raw_vs_filtered, plot_red_raw_vs_filtered


METHODS = {
    "current": CurrentPeakAutocorr,
    "median_butterworth": MedianButterworthPeakAutocorr,
}


def run_pipeline(csv_path, method_name, fs=100, collect_debug=False, plot_seconds=20):
    if method_name not in METHODS:
        raise ValueError(f"Unknown method: {method_name}. Available: {list(METHODS.keys())}")

    df = load_dataset(csv_path)

    method = METHODS[method_name](fs=fs)
    dataset_name = os.path.basename(csv_path)

    results = []
    debug_rows = []

    max_debug_samples = int(plot_seconds * fs)

    for start in range(0, len(df) - BUFFER_SIZE + 1, BUFFER_SIZE):
        chunk = df.iloc[start:start + BUFFER_SIZE]

        output = method.process_chunk(chunk)

        if output is None:
            continue

        results.append({
            "dataset": dataset_name,
            "method": method.name,
            "time_s": start / fs,
            "hr": output["hr"],
            "peak_conf": output["peak_conf"],
            "auto_conf": output["auto_conf"],
        })

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

    args = parser.parse_args()

    result, debug = run_pipeline(
        csv_path=args.input,
        method_name=args.method,
        fs=args.fs,
        collect_debug=args.plot,
        plot_seconds=args.plot_seconds,
    )

    metrics = calculate_metrics(result)

    print(result)

    print("\n=== BASIC METRICS ===")
    print("DATASET:", os.path.basename(args.input))
    print("METHOD:", args.method)

    for key, value in metrics.items():
        print(f"{key}: {value}")

    if args.out:
        result.to_csv(args.out, index=False)

    if args.plot and len(debug) > 0:
        dataset_name = os.path.basename(args.input)

        if args.plot_out:
            base_name = os.path.splitext(dataset_name)[0]
            ir_out = os.path.join(args.plot_out, f"{base_name}_{args.method}_ir.png")
            red_out = os.path.join(args.plot_out, f"{base_name}_{args.method}_red.png")
        else:
            ir_out = None
            red_out = None

        plot_raw_vs_filtered(
            debug,
            title=f"{dataset_name} | {args.method} | IR raw vs filtered",
            out_path=ir_out
        )

        plot_red_raw_vs_filtered(
            debug,
            title=f"{dataset_name} | {args.method} | RED raw vs filtered",
            out_path=red_out
        )