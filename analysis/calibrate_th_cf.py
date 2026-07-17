import os
import argparse
import numpy as np
import pandas as pd

from scipy.fft import rfft, rfftfreq
from scipy.signal import butter, sosfilt, windows, find_peaks

from methods.common import MedianOnlyChannel
from utils.data_loader import load_dataset


def median_filter_ir(ir_raw):
    median = MedianOnlyChannel()
    result = []

    for sample in ir_raw:
        result.append(median.process(int(sample)))

    return np.asarray(result, dtype=float)


def butterworth_filter(signal, fs=100, low=0.4, high=4.0, order=2):
    sos = butter(
        order,
        [low, high],
        btype="bandpass",
        fs=fs,
        output="sos"
    )

    return sosfilt(sos, signal)


def calculate_window_cf(window_signal, fs=100, n_fft=2048):
    window_signal = np.asarray(window_signal, dtype=float)

    window_signal = window_signal - np.mean(window_signal)

    hann = windows.hann(len(window_signal))
    window_signal = window_signal * hann

    fft_values = rfft(window_signal, n=n_fft)

    # Periodogram / power spectrum
    spec = np.abs(fft_values) ** 2
    freq = rfftfreq(n_fft, d=1 / fs)

    mask = (freq >= 0.67) & (freq <= 3.3)

    freq_range = freq[mask]
    spec_range = spec[mask]

    if len(spec_range) == 0:
        return None

    min_val = np.min(spec_range)
    max_val = np.max(spec_range)

    if max_val - min_val <= 1e-12:
        return None

    # Normalized periodogram: min = 0, max = 1
    spec_norm = (spec_range - min_val) / (max_val - min_val)

    rms = np.sqrt(np.mean(spec_norm ** 2))

    if rms <= 1e-12:
        return None

    peaks, _ = find_peaks(
        spec_norm,
        distance=2,
        height=0.10,
        prominence=0.03
    )

    if len(peaks) == 0:
        return None

    peak_heights = spec_norm[peaks]
    peak_freqs = freq_range[peaks]

    winner_idx = int(np.argmax(peak_heights))

    winner_peak = float(peak_heights[winner_idx])
    winner_freq = float(peak_freqs[winner_idx])
    winner_hr = winner_freq * 60.0

    # Prawdziwy Crest Factor
    winner_cf = winner_peak / rms

    return {
        "cf": float(winner_cf),
        "hr": float(winner_hr),
        "freq_hz": float(winner_freq),
        "peak_norm": float(winner_peak),
        "rms": float(rms),
    }


def calibrate_th_cf(
    csv_path,
    fs=100,
    fft_buffer=1024,
    chunk_size=256,
    n_fft=2048,
    factor=0.70
):
    df = load_dataset(csv_path)

    ir_raw = df["ir_raw"].to_numpy(dtype=int)

    ir_median = median_filter_ir(ir_raw)
    ir_filtered = butterworth_filter(ir_median, fs=fs)

    rows = []

    for start in range(0, len(ir_filtered) - fft_buffer + 1, chunk_size):
        window_signal = ir_filtered[start:start + fft_buffer]

        result = calculate_window_cf(
            window_signal,
            fs=fs,
            n_fft=n_fft
        )

        if result is None:
            continue

        rows.append({
            "time_s": start / fs,
            "hr": result["hr"],
            "freq_hz": result["freq_hz"],
            "cf": result["cf"],
            "peak_norm": result["peak_norm"],
            "rms": result["rms"],
        })

    result_df = pd.DataFrame(rows)

    if len(result_df) == 0:
        raise RuntimeError("Nie znaleziono żadnych poprawnych pików CF.")

    mean_cf = float(result_df["cf"].mean())
    median_cf = float(result_df["cf"].median())
    min_cf = float(result_df["cf"].min())
    max_cf = float(result_df["cf"].max())

    th_cf = mean_cf * factor

    print("\n=== TH_CF CALIBRATION ===")
    print(f"windows_used: {len(result_df)}")
    print(f"mean_cf: {mean_cf:.4f}")
    print(f"median_cf: {median_cf:.4f}")
    print(f"min_cf: {min_cf:.4f}")
    print(f"max_cf: {max_cf:.4f}")
    print(f"factor: {factor:.2f}")
    print(f"TH_CF = mean_cf * factor: {th_cf:.4f}")

    print("\n=== CF VALUES PER WINDOW ===")
    print(result_df)

    return result_df, th_cf


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--input", required=True)
    parser.add_argument("--fs", type=int, default=100)
    parser.add_argument("--fft-buffer", type=int, default=1024)
    parser.add_argument("--chunk-size", type=int, default=256)
    parser.add_argument("--n-fft", type=int, default=2048)
    parser.add_argument("--factor", type=float, default=0.70)
    parser.add_argument("--out", default=None)

    args = parser.parse_args()

    result_df, th_cf = calibrate_th_cf(
        csv_path=args.input,
        fs=args.fs,
        fft_buffer=args.fft_buffer,
        chunk_size=args.chunk_size,
        n_fft=args.n_fft,
        factor=args.factor,
    )

    if args.out:
        out_dir = os.path.dirname(args.out)

        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        result_df.to_csv(args.out, index=False)
        print(f"\nSaved: {args.out}")