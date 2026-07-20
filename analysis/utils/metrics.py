"""
Metric utilities for evaluating the HR estimation algorithm

Calculates key quality metrics, including valid ratio, MAE, RMSE, bias,
maximum error, HR variability, jump/spike count, and accuracy within selected
BPM or percentage thresholds compared with reference HR
"""

import numpy as np


def calculate_metrics(result_df):
    if result_df is None or len(result_df) == 0:
        return {
            "valid_ratio": 0.0,
            "median_hr": 0.0,
            "min_hr": 0.0,
            "max_hr": 0.0,
            "hr_mad": 0.0,
            "max_jump": 0.0,
            "spike_count_15bpm": 0,
            "median_local_deviation": 0.0,
        }

    df = result_df.copy()
    valid = df[df["hr"] > 0].copy()

    if len(valid) == 0:
        return {
            "valid_ratio": 0.0,
            "median_hr": 0.0,
            "min_hr": 0.0,
            "max_hr": 0.0,
            "hr_mad": 0.0,
            "max_jump": 0.0,
            "spike_count_15bpm": 0,
            "median_local_deviation": 0.0,
        }

    valid_ratio = len(valid) / len(df)

    hr = valid["hr"].astype(float)

    median_hr = hr.median()
    min_hr = hr.min()
    max_hr = hr.max()

    hr_mad = (hr - median_hr).abs().median()

    jumps = hr.diff().abs().dropna()

    if len(jumps) > 0:
        max_jump = jumps.max()
        spike_count_15bpm = int((jumps > 15).sum())
    else:
        max_jump = 0.0
        spike_count_15bpm = 0

    local_median = hr.rolling(
        window=5,
        center=True,
        min_periods=1
    ).median()

    local_deviation = (hr - local_median).abs()
    median_local_deviation = local_deviation.median()

    return {
        "valid_ratio": float(valid_ratio),
        "median_hr": float(median_hr),
        "min_hr": float(min_hr),
        "max_hr": float(max_hr),
        "hr_mad": float(hr_mad),
        "max_jump": float(max_jump),
        "spike_count_15bpm": int(spike_count_15bpm),
        "median_local_deviation": float(median_local_deviation) if not np.isnan(median_local_deviation) else 0.0,
    }


def calculate_reference_metrics(result_df):
    if result_df is None or len(result_df) == 0:
        return {}

    if "ref_hr" not in result_df.columns:
        return {}

    df = result_df.copy()
    ref_df = df[df["ref_hr"] > 0].copy()

    if len(ref_df) == 0:
        return {
            "ref_windows": 0,
        }

    def metrics_for_column(col_name, prefix):
        if col_name not in ref_df.columns:
            return {}

        valid = ref_df[ref_df[col_name] > 0].copy()

        out = {}

        out[f"{prefix}_valid_windows"] = int(len(valid))
        out[f"{prefix}_invalid_windows"] = int(len(ref_df) - len(valid))
        out[f"{prefix}_valid_ratio"] = float(len(valid) / len(ref_df))

        if len(valid) == 0:
            out[f"{prefix}_mae"] = 0.0
            out[f"{prefix}_median_abs_error"] = 0.0
            out[f"{prefix}_rmse"] = 0.0
            out[f"{prefix}_bias"] = 0.0
            out[f"{prefix}_max_abs_error"] = 0.0
            out[f"{prefix}_p90_abs_error"] = 0.0
            out[f"{prefix}_p95_abs_error"] = 0.0
            out[f"{prefix}_within_3bpm_ratio"] = 0.0
            out[f"{prefix}_within_5bpm_ratio"] = 0.0
            out[f"{prefix}_within_10bpm_ratio"] = 0.0
            out[f"{prefix}_within_10pct_or_5bpm_ratio"] = 0.0
            out[f"{prefix}_effective_within_5bpm_ratio"] = 0.0
            out[f"{prefix}_effective_within_10pct_or_5bpm_ratio"] = 0.0
            return out

        hr = valid[col_name].astype(float)
        ref = valid["ref_hr"].astype(float)

        error = hr - ref
        abs_error = error.abs()

        threshold_10pct_or_5 = np.maximum(5.0, 0.10 * ref)

        within_3 = abs_error <= 3.0
        within_5 = abs_error <= 5.0
        within_10 = abs_error <= 10.0
        within_10pct_or_5 = abs_error <= threshold_10pct_or_5

        out[f"{prefix}_mae"] = float(abs_error.mean())
        out[f"{prefix}_median_abs_error"] = float(abs_error.median())
        out[f"{prefix}_rmse"] = float(np.sqrt(np.mean(error ** 2)))
        out[f"{prefix}_bias"] = float(error.mean())
        out[f"{prefix}_max_abs_error"] = float(abs_error.max())
        out[f"{prefix}_p90_abs_error"] = float(np.percentile(abs_error, 90))
        out[f"{prefix}_p95_abs_error"] = float(np.percentile(abs_error, 95))

        out[f"{prefix}_underestimate_count"] = int((error < 0).sum())
        out[f"{prefix}_overestimate_count"] = int((error > 0).sum())

        out[f"{prefix}_within_3bpm_ratio"] = float(within_3.mean())
        out[f"{prefix}_within_5bpm_ratio"] = float(within_5.mean())
        out[f"{prefix}_within_10bpm_ratio"] = float(within_10.mean())
        out[f"{prefix}_within_10pct_or_5bpm_ratio"] = float(within_10pct_or_5.mean())

        out[f"{prefix}_effective_within_5bpm_ratio"] = float(within_5.sum() / len(ref_df))
        out[f"{prefix}_effective_within_10pct_or_5bpm_ratio"] = float(within_10pct_or_5.sum() / len(ref_df))

        return out

    metrics = {
        "ref_windows": int(len(ref_df)),
    }

    metrics.update(metrics_for_column("hr", "display"))

    metrics.update(metrics_for_column("raw_hr", "raw"))

    metrics.update(metrics_for_column("last_stable_hr", "stable"))

    if "display_mae" in metrics:
        metrics["valid_ref_windows"] = metrics["display_valid_windows"]
        metrics["ref_valid_ratio"] = metrics["display_valid_ratio"]
        metrics["mae"] = metrics["display_mae"]
        metrics["median_abs_error"] = metrics["display_median_abs_error"]
        metrics["rmse"] = metrics["display_rmse"]
        metrics["bias"] = metrics["display_bias"]
        metrics["max_abs_error"] = metrics["display_max_abs_error"]
        metrics["within_5bpm_ratio"] = metrics["display_within_5bpm_ratio"]
        metrics["within_10pct_or_5bpm_ratio"] = metrics["display_within_10pct_or_5bpm_ratio"]
        metrics["effective_accurate_ratio"] = metrics["display_effective_within_10pct_or_5bpm_ratio"]

    return metrics