"""
FFT and FSM based HR estimation method

Algorithm looks for HR candidates in the PPG signal and compares them with
motion information from the IMU. Motion-related candidates are penalized, and a
finite-state machine decides whether the HR result should be accepted, held, or
rejected
"""

import numpy as np
from scipy.fft import rfft, rfftfreq
from scipy.signal import butter, sosfilt, sosfilt_zi, windows, find_peaks
from methods.common import MedianOnlyChannel
from utils.ring_buffer import RingBuffer


class FFT_FSM:
    name = "fft_fsm_baseline"

    def __init__(self, buffer=1024, chunk_size=256, fs=100, low=0.4, high=4, order=2, n_fft=2048):
        self.fs = fs
        self.buffer = buffer
        self.chunk_size = chunk_size
        self.n_fft = n_fft

        self.last_score = 0.0
        self.last_motion_score = 0.0
        self.last_raw_hr = 0.0

        self.current_state = "STABLE"
        self.last_stable_hr = 0
        self.last_hr = 0
        self.display_hr = 0.0
        self.second_last_hr = 0
        self.TH_CF = 3.4
        self.MAX_DIFF = 6
        self.last_cf = 0.0
        self.HR_SMOOTH_ALPHA = 0.2
        self.penalty_weight = 1.10
        self.bonus_weight = 0.015

        self.alert_counter = 0
        self.recovery_counter = 0
        self.good_windows = 0

        self.ir_ring = RingBuffer(buffer)

        self.accX_ring = RingBuffer(buffer)
        self.accY_ring = RingBuffer(buffer)
        self.accZ_ring = RingBuffer(buffer)

        self.gyroX_ring = RingBuffer(buffer)
        self.gyroY_ring = RingBuffer(buffer)
        self.gyroZ_ring = RingBuffer(buffer)

        self.ir_median = MedianOnlyChannel()
        self.red_median = MedianOnlyChannel()

        self.accX_median = MedianOnlyChannel()
        self.accY_median = MedianOnlyChannel()
        self.accZ_median = MedianOnlyChannel()

        self.gyroX_median = MedianOnlyChannel()
        self.gyroY_median = MedianOnlyChannel()
        self.gyroZ_median = MedianOnlyChannel()

        self.sos_ir = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
        self.sos_accX = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
        self.sos_accY = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
        self.sos_accZ = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
        self.sos_gyroX = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
        self.sos_gyroY = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")
        self.sos_gyroZ = butter(order, [low, high], btype="bandpass", fs=fs, output="sos")

        self.zi_accX = None
        self.zi_accY = None
        self.zi_accZ = None
        self.zi_gyroX = None
        self.zi_gyroY = None
        self.zi_gyroZ = None
        self.zi_ir = None

    def process_fft(self, signal):
        signal = np.asarray(signal, dtype=float)
        signal = signal - np.mean(signal)

        window = windows.hann(len(signal))
        signal = signal * window

        # periodogram
        spec = np.abs(rfft(signal, n=self.n_fft)) ** 2
        freq = rfftfreq(self.n_fft, d=1 / self.fs)

        return spec, freq

    def detect_hr_frequencies(self, clean_ir):
        ir_spec, ir_freq = self.process_fft(clean_ir)
        mask = (ir_freq >= 0.67) & (ir_freq <= 3.3)

        freq_range = ir_freq[mask]
        spec_range = ir_spec[mask]

        if len(spec_range) == 0:
            return np.array([]), np.array([]), np.array([])

        max_val = np.max(spec_range)
        min_val = np.min(spec_range)

        if max_val <= 1e-9:
            return np.array([]), np.array([]), np.array([])

        spec_norm = (spec_range - min_val) / (max_val - min_val)
        rms = np.sqrt(np.mean(spec_norm ** 2))

        if rms <= 1e-12:
            return np.array([]), np.array([]), np.array([])

        peaks, _ = find_peaks(spec_norm, distance=2, height=0.12, prominence=0.05)

        if len(peaks) == 0:
            return np.array([]), np.array([]), np.array([])

        peak_freq = freq_range[peaks]
        peak_spec = spec_norm[peaks]

        peak_cf = peak_spec / rms

        order = np.argsort(peak_spec)[::-1]

        top_freq = peak_freq[order[:5]]
        top_spec = peak_spec[order[:5]]
        top_cf = peak_cf[order[:5]]

        return top_spec, top_freq, top_cf


    def detect_motion_frequencies(self, clean_accx, clean_accy, clean_accz,
                                  clean_gyrox, clean_gyroy, clean_gyroz):
        accx_spec, freq = self.process_fft(clean_accx)
        accy_spec, _ = self.process_fft(clean_accy)
        accz_spec, _ = self.process_fft(clean_accz)

        gyrox_spec, _ = self.process_fft(clean_gyrox)
        gyroy_spec, _ = self.process_fft(clean_gyroy)
        gyroz_spec, _ = self.process_fft(clean_gyroz)

        acc_sum = accx_spec + accy_spec + accz_spec
        gyro_sum = gyrox_spec + gyroy_spec + gyroz_spec

        mask = (freq >= 0.67) & (freq <= 3.3)
        freq_range = freq[mask]

        acc_band = acc_sum[mask]
        gyro_band = gyro_sum[mask]

        if len(freq_range) == 0:
            return np.array([]), np.array([])

        scale_acc = np.percentile(acc_band, 95)
        scale_gyro = np.percentile(gyro_band, 95)

        if scale_acc <= 1e-9:
            acc_norm = np.zeros_like(acc_band)
        else:
            acc_norm = np.clip(acc_band / scale_acc, 0.0, 1.0)

        if scale_gyro <= 1e-9:
            gyro_norm = np.zeros_like(gyro_band)
        else:
            gyro_norm = np.clip(gyro_band / scale_gyro, 0.0, 1.0)

        motion_spec = np.maximum(acc_norm, gyro_norm)

        return motion_spec, freq_range


    def calculate_hr_FSM(self, ir_top_spec, ir_top_freq, ir_top_cf,
                         imu_spec, imu_freq, penalty_weight=None, bonus_weight=None):

        if penalty_weight is None:
            penalty_weight = self.penalty_weight

        if bonus_weight is None:
            bonus_weight = self.bonus_weight

        if len(ir_top_spec) == 0 or len(imu_spec) == 0:
            if self.last_stable_hr > 0:
                self.current_state = "ALERT"
                self.alert_counter += 1

                if self.alert_counter >= 5:
                    self.current_state = "UNCERTAIN"
                    return 0

                return self.last_stable_hr

            return 0

        scores = []
        self.last_cf = 0.0

        window_bins = max(1, int(self.n_fft / 1024))

        for s in range(len(ir_top_spec)):
            candidate_hr = ir_top_freq[s] * 60
            imu_idx = np.argmin(np.abs(imu_freq - ir_top_freq[s]))

            start = max(0, imu_idx - window_bins)
            end = min(len(imu_spec), imu_idx + window_bins + 1)

            imu_val = np.max(imu_spec[start:end])

            bonus = 0
            if self.last_stable_hr > 0:
                diff = abs(candidate_hr - self.last_stable_hr)
                if diff < 15:
                    bonus = (15 - diff) * bonus_weight

            score_val = ir_top_spec[s] - (imu_val * penalty_weight) + bonus
            scores.append(score_val)

        winner = scores.index(max(scores))
        cf_winner = ir_top_cf[winner]
        self.last_cf = float(cf_winner)
        hr_winner = ir_top_freq[winner] * 60

        self.last_score = float(scores[winner])
        self.last_motion_score = float(
            np.max(
                imu_spec[
                max(0, np.argmin(np.abs(imu_freq - ir_top_freq[winner])) - window_bins):
                min(len(imu_spec), np.argmin(np.abs(imu_freq - ir_top_freq[winner])) + window_bins + 1)
                ]
            )
        )
        self.last_raw_hr = float(hr_winner)

        is_signal_sharp = cf_winner > self.TH_CF
        hr_jump = abs(hr_winner - self.last_stable_hr)
        recovery_jump = abs(hr_winner - self.last_hr)
        second_recovery_jump = abs(self.last_hr - self.second_last_hr)

        if self.current_state == "STABLE":
            if is_signal_sharp and (hr_jump <= self.MAX_DIFF or self.last_stable_hr == 0):
                self.last_stable_hr = hr_winner
                self.second_last_hr = self.last_hr
                self.last_hr = hr_winner
                return self.last_stable_hr
            else:
                self.current_state = "ALERT"
                self.alert_counter = 0
                self.second_last_hr = self.last_hr
                self.last_hr = hr_winner
                return self.last_stable_hr

        elif self.current_state == "ALERT":
            if is_signal_sharp:
                self.current_state = "RECOVERY"
                self.recovery_counter = 0
                self.second_last_hr = self.last_hr
                self.last_hr = hr_winner
                return self.last_stable_hr
            else:
                self.alert_counter += 1
                if self.alert_counter >= 5:
                    self.current_state = "UNCERTAIN"
                    self.alert_counter = 0
                    return 0

                return self.last_stable_hr

        elif self.current_state == "UNCERTAIN":
            if is_signal_sharp:
                self.good_windows += 1
                if self.good_windows >= 3:
                    self.current_state = "ALERT"
                    self.good_windows = 0
                    return 0
                else:

                    return 0
            else:
                self.good_windows = 0

                return 0

        elif self.current_state == "RECOVERY":
            if is_signal_sharp:
                self.recovery_counter += 1
                if self.recovery_counter >= 4:
                    if recovery_jump <= self.MAX_DIFF and second_recovery_jump <= self.MAX_DIFF:
                        self.current_state = "STABLE"
                        self.last_stable_hr = hr_winner
                        self.second_last_hr = self.last_hr
                        self.last_hr = hr_winner
                        self.recovery_counter = 0
                        return self.last_stable_hr
                    else:
                        self.recovery_counter = 0

                self.second_last_hr = self.last_hr
                self.last_hr = hr_winner
                return self.last_stable_hr
            else:
                self.current_state = "ALERT"
                self.recovery_counter = 0

                return 0


    def smooth_hr(self, hr):
        if hr <= 0:
            return 0

        if self.display_hr <= 0:
            self.display_hr = float(hr)
        else:
            self.display_hr = (
                    self.display_hr
                    + self.HR_SMOOTH_ALPHA * (float(hr) - self.display_hr)
            )

        return self.display_hr


    def process_chunk(self, chunk):
        if len(chunk) != self.chunk_size:
            return None

        self.ir_median.resetSum()

        ir_clean = []
        accX_clean = []
        accY_clean = []
        accZ_clean = []
        gyroX_clean = []
        gyroY_clean = []
        gyroZ_clean = []

        ir_raw_values = []
        red_raw_values = []

        for _, row in chunk.iterrows():
            ir_raw = int(row["ir_raw"])
            red_raw = int(row["red_raw"])

            acc_x = int(row["acc_x"])
            acc_y = int(row["acc_y"])
            acc_z = int(row["acc_z"])
            gyro_x = int(row["gyro_x"])
            gyro_y = int(row["gyro_y"])
            gyro_z = int(row["gyro_z"])

            ir_raw_values.append(ir_raw)
            red_raw_values.append(red_raw)

            ir_clean.append(self.ir_median.process(ir_raw))

            accX_clean.append(self.accX_median.process(acc_x))
            accY_clean.append(self.accY_median.process(acc_y))
            accZ_clean.append(self.accZ_median.process(acc_z))

            gyroX_clean.append(self.gyroX_median.process(gyro_x))
            gyroY_clean.append(self.gyroY_median.process(gyro_y))
            gyroZ_clean.append(self.gyroZ_median.process(gyro_z))

        ir_clean = np.asarray(ir_clean, dtype=float)

        accX_clean = np.asarray(accX_clean, dtype=float)
        accY_clean = np.asarray(accY_clean, dtype=float)
        accZ_clean = np.asarray(accZ_clean, dtype=float)

        gyroX_clean = np.asarray(gyroX_clean, dtype=float)
        gyroY_clean = np.asarray(gyroY_clean, dtype=float)
        gyroZ_clean = np.asarray(gyroZ_clean, dtype=float)

        if self.zi_ir is None:
            self.zi_ir = sosfilt_zi(self.sos_ir) * ir_clean[0]

        if self.zi_accX is None:
            self.zi_accX = sosfilt_zi(self.sos_accX) * accX_clean[0]
        if self.zi_accY is None:
            self.zi_accY = sosfilt_zi(self.sos_accY) * accY_clean[0]
        if self.zi_accZ is None:
            self.zi_accZ = sosfilt_zi(self.sos_accZ) * accZ_clean[0]

        if self.zi_gyroX is None:
            self.zi_gyroX = sosfilt_zi(self.sos_gyroX) * gyroX_clean[0]
        if self.zi_gyroY is None:
            self.zi_gyroY = sosfilt_zi(self.sos_gyroY) * gyroY_clean[0]
        if self.zi_gyroZ is None:
            self.zi_gyroZ = sosfilt_zi(self.sos_gyroZ) * gyroZ_clean[0]

        ir_ac, self.zi_ir = sosfilt(self.sos_ir, ir_clean, zi=self.zi_ir)

        accel_X_filtered, self.zi_accX = sosfilt(self.sos_accX, accX_clean, zi=self.zi_accX)
        accel_Y_filtered, self.zi_accY = sosfilt(self.sos_accY, accY_clean, zi=self.zi_accY)
        accel_Z_filtered, self.zi_accZ = sosfilt(self.sos_accZ, accZ_clean, zi=self.zi_accZ)

        gyro_X_filtered, self.zi_gyroX = sosfilt(self.sos_gyroX, gyroX_clean, zi=self.zi_gyroX)
        gyro_Y_filtered, self.zi_gyroY = sosfilt(self.sos_gyroY, gyroY_clean, zi=self.zi_gyroY)
        gyro_Z_filtered, self.zi_gyroZ = sosfilt(self.sos_gyroZ, gyroZ_clean, zi=self.zi_gyroZ)

        self.ir_ring.append(ir_ac)

        self.accX_ring.append(accel_X_filtered)
        self.accY_ring.append(accel_Y_filtered)
        self.accZ_ring.append(accel_Z_filtered)

        self.gyroX_ring.append(gyro_X_filtered)
        self.gyroY_ring.append(gyro_Y_filtered)
        self.gyroZ_ring.append(gyro_Z_filtered)

        if not self.ir_ring.ready():
            display_heartRate = 0
        else:
            ir_fft_signal = self.ir_ring.get_ordered()
            accX_fft_signal = self.accX_ring.get_ordered()
            accY_fft_signal = self.accY_ring.get_ordered()
            accZ_fft_signal = self.accZ_ring.get_ordered()
            gyroX_fft_signal = self.gyroX_ring.get_ordered()
            gyroY_fft_signal = self.gyroY_ring.get_ordered()
            gyroZ_fft_signal = self.gyroZ_ring.get_ordered()

            hr_top_spec, hr_top_freq, hr_top_cf = self.detect_hr_frequencies(ir_fft_signal)
            imu_spec, imu_freq = self.detect_motion_frequencies(accX_fft_signal, accY_fft_signal, accZ_fft_signal,
                                                                gyroX_fft_signal, gyroY_fft_signal, gyroZ_fft_signal)

            heartRate = self.calculate_hr_FSM(hr_top_spec, hr_top_freq, hr_top_cf, imu_spec, imu_freq,
                                              self.penalty_weight, self.bonus_weight)
            display_heartRate = self.smooth_hr(heartRate)

        return {
            "hr": display_heartRate,
            "peak_conf": self.last_cf,
            "auto_conf": 0,
            "state": self.current_state,
            "raw_hr": self.last_raw_hr,
            "last_stable_hr": self.last_stable_hr,
            "score": self.last_score,
            "motion_score": self.last_motion_score,
            "debug": {
                "ir_raw": np.asarray(ir_raw_values, dtype=np.int32),
                "red_raw": np.asarray(red_raw_values, dtype=np.int32),
                "ir_filtered": np.asarray(ir_ac, dtype=np.int32),
                "red_filtered": np.zeros_like(ir_ac, dtype=np.int32),
            }
        }