# Validation Overview

The validation stage evaluates the complete measurement pipeline using recorded sensor data collected under different motion conditions.

7 datasets were recorded, ranging from rest and controlled arm movements to walking, jogging and random whole-body motion.

The validation focuses on two outputs:

- heart-rate estimation,
- SpO2 estimation.

## Heart-Rate Validation

Heart-rate results are compared with measurements recorded using an ECG chest strap.

Both the raw HR output produced by the state machine and the final smoothed HR are evaluated.

The analysis includes:

- result availability,
- MAE and RMSE,
- bias and maximum error,
- percentage of estimates within ±5 BPM,
- percentage of estimates within ±10% or 5 BPM,
- HR tracking over time,
- aggregate agreement with the ECG reference.

The detailed results are available in [heart_rate.md](heart_rate.md).

## SpO2 Validation

SpO2 is evaluated using the same recorded datasets.

No independent reference pulse oximeter was recorded, so the current validation does not measure absolute SpO2 accuracy.

Instead, it evaluates:

- result availability,
- stability of valid estimates,
- behavior under different motion conditions.

Valid SpO2 results remained within approximately **99–100%** in the recorded datasets.

The detailed results are available in [spo2.md](spo2.md).

## Validation Data

Descriptions of all recorded datasets, activity conditions and recording durations are available in [datasets.md](datasets.md).

The recordings include:

- rest,
- linear arm movements,
- rotational arm movements,
- marching in place,
- brisk walking,
- jogging,
- random motion.

Together, these datasets provide progressively more difficult conditions for evaluating the influence of motion on PPG-based measurements.