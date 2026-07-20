"""
Baseline HR estimation method using peak detection and autocorrelation

This class tests the peak-autocorrelation algorithm based on the microcontroller implementation
"""

import numpy as np

from methods.common import BUFFER_SIZE, OpticalChannel, PeakAutocorrHR


class CurrentPeakAutocorr:
    name = "peak_autocorr_baseline"

    def __init__(self, fs=100):
        self.fs = fs

        self.ir_channel = OpticalChannel()
        self.red_channel = OpticalChannel()

        self.hr_estimator = PeakAutocorrHR(fs=fs)

    def process_chunk(self, chunk):
        self.ir_channel.resetSum()
        self.red_channel.resetSum()

        ac_ir = []
        ac_red = []

        ir_raw_values = []
        red_raw_values = []

        for _, row in chunk.iterrows():
            ir_raw = int(row["ir_raw"])
            red_raw = int(row["red_raw"])

            ir_raw_values.append(ir_raw)
            red_raw_values.append(red_raw)

            ir_ac = self.ir_channel.process(ir_raw)
            red_ac = self.red_channel.process(red_raw)

            ac_ir.append(ir_ac)
            ac_red.append(red_ac)

        if len(ac_ir) != BUFFER_SIZE:
            return None

        processed = {
            "acIr": np.asarray(ac_ir, dtype=np.int32),
            "acRed": np.asarray(ac_red, dtype=np.int32),
            "dcIr": self.ir_channel.getSum() // BUFFER_SIZE,
            "dcRed": self.red_channel.getSum() // BUFFER_SIZE,
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