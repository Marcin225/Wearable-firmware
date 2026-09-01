# Testing Overview

The test suite verifies the signal-processing pipeline at several levels, from individual algorithms to the complete HR and SpO2 processing path.

The tests are divided into complementary layers:

- C++ unit tests,
- C++ pipeline wrappers,
- Python reference implementations,
- Python validators,
- parameter sweeps.

Each layer checks a different part of the firmware and helps separate implementation errors from algorithmic errors.

## C++ Unit Tests

C++ unit tests verify individual production functions and processing stages in isolation.

The project currently contains two Unity-based unit-test groups:

- motion spectrum calculation,
- SpO2 estimation.

The motion tests verify frequency mapping, normalization and combined spectral power from the three accelerometer axes.

The SpO2 tests verify invalid-input handling, supported ratio limits and continuity of the SpO2 lookup table.

The tests use the same production modules and configuration values as the firmware instead of duplicating the processing logic.

## C++ Pipeline Wrappers

C++ wrappers execute larger parts of the production processing pipeline on recorded sensor data.

They are used to verify how multiple processing stages work together.

## Python Reference Implementations

The signal-processing algorithms were first developed and evaluated in Python before being implemented in the firmware.

The Python versions served as the initial reference implementations for:

- filtering,
- FFT processing,
- HR candidate detection,
- HR estimation.

After the algorithms were transferred to C++ and rewritten using fixed-point arithmetic, the Python implementations were retained as reference models.

They are now used to compare the firmware implementation with the original algorithm behavior and to help identify differences introduced during the fixed-point conversion.

The Python implementations are intended for development, verification and analysis rather than for use in the embedded firmware.

## Python Validators

Python validators analyze output produced by the C++ processing wrappers.

Depending on the test, the C++ results are compared either with:

- the corresponding Python reference implementation,
- reference heart-rate data recorded with the ECG chest strap.

The validators calculate numerical differences and accuracy metrics and check whether the firmware implementation behaves as expected.

## Parameter Sweeps

Parameter sweep tools run the HR algorithm repeatedly with different configuration values.

They are used to tune parameters such as:

- `BONUS_Q12`
- `MAIN_PENALTY_Q12`
- `TH_CF_Q12`

Each parameter set is evaluated against recorded reference data, allowing the configuration to be selected based on measured performance rather than manual trial and error.

## Test Structure

```text
analysis/
├── test/
│   ├── test_pre_processing_filters.py
│   ├── test_rfft.py
│   └── test_hr_candidates_designating.py
├── methods/
├── utils/
└── calibrate_th_cf.py

test/
├── preprocessing/
├── rfft/
├── hr_candidates/
├── hr_estimation/
├── unit/
└── vitals_pipeline/
```

The `analysis/` directory contains the original Python implementations and development scripts used during algorithm design and calibration.

The `test/` directory contains C++ processing wrappers, Unity unit tests and Python validators used to compare the firmware implementation with the Python reference versions or ECG chest-strap measurements.