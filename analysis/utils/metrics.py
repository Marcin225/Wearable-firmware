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