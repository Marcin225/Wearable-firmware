import numpy as np
from scipy.signal import butter, sosfilt, sosfilt_zi

from methods.common import (
    BUFFER_SIZE,
    MedianOnlyChannel,
    PeakAutocorrHR,
)


class MedianButterworthPeakAutocorr:
    name = "median_butterworth_baseline"

    def __init__(self, fs=100, low=0.4, high=4, order=2):
        self.fs = fs

        self.ir_median = MedianOnlyChannel()
        self.red_median = MedianOnlyChannel()

        self.sos_ir = butter(
            order,
            [low, high],
            btype="bandpass",
            fs=fs,
            output="sos"
        )

        self.sos_red = butter(
            order,
            [low, high],
            btype="bandpass",
            fs=fs,
            output="sos"
        )

        self.zi_ir = None
        self.zi_red = None

        self.hr_estimator = PeakAutocorrHR(fs=fs)

    def process_chunk(self, chunk):
        if len(chunk) != BUFFER_SIZE:
            return None

        self.ir_median.resetSum()
        self.red_median.resetSum()

        ir_clean = []
        red_clean = []

        ir_raw_values = []
        red_raw_values = []

        for _, row in chunk.iterrows():
            ir_raw = int(row["ir_raw"])
            red_raw = int(row["red_raw"])

            ir_clean.append(self.ir_median.process(ir_raw))
            red_clean.append(self.red_median.process(red_raw))

            ir_raw_values.append(ir_raw)
            red_raw_values.append(red_raw)

        ir_clean = np.asarray(ir_clean, dtype=float)
        red_clean = np.asarray(red_clean, dtype=float)

        dc_ir = self.ir_median.getSum() // BUFFER_SIZE
        dc_red = self.red_median.getSum() // BUFFER_SIZE

        if self.zi_ir is None:
            self.zi_ir = sosfilt_zi(self.sos_ir) * ir_clean[0]

        if self.zi_red is None:
            self.zi_red = sosfilt_zi(self.sos_red) * red_clean[0]

        ir_ac, self.zi_ir = sosfilt(self.sos_ir, ir_clean, zi=self.zi_ir)
        red_ac, self.zi_red = sosfilt(self.sos_red, red_clean, zi=self.zi_red)

        processed = {
            "acIr": np.asarray(ir_ac, dtype=np.int32),
            "acRed": np.asarray(red_ac, dtype=np.int32),
            "dcIr": int(dc_ir),
            "dcRed": int(dc_red),
        }

        hr, peak_conf, auto_conf = self.hr_estimator.calculate(processed)

        return {
            "hr": hr,
            "peak_conf": peak_conf,
            "auto_conf": auto_conf,
             "debug": {
                "ir_raw": np.asarray(ir_raw_values, dtype=np.int32),
                "red_raw": np.asarray(red_raw_values, dtype=np.int32),
                "ir_filtered": processed["acIr"],
                "red_filtered": processed["acRed"],
            }
        }