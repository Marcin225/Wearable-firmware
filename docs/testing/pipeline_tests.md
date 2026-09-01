# Pipeline Tests

Pipeline tests execute production signal-processing code on recorded sensor data outside the ESP32-C3 runtime.

Unlike isolated unit tests, these tests verify complete processing stages or combinations of several stages using real measurement samples.

The C++ wrappers use the same production modules, configuration values and fixed-point arithmetic as the firmware.

## Preprocessing Pipeline

The preprocessing wrapper applies the production filtering pipeline to recorded IR and accelerometer data.

The generated IR, AccX, AccY and AccZ signals are compared sample-by-sample with the original Python reference implementation.

Because the C++ implementation uses fixed-point arithmetic while the reference implementation uses Python numerical processing, the validator evaluates both numerical error and signal-shape similarity.

The current acceptance limits are:

| Metric | Requirement |
| :--- | :--- |
| NRMSE | ≤ 10% |
| Correlation | ≥ 0.98 |
| Samples within tolerance | ≥ 95% |
| Sample tolerance | `5 + 5%` of reference magnitude |

MAE, RMSE and maximum absolute error are also reported for diagnostic purposes but are not used directly as pass/fail conditions.

All tested channels must satisfy the acceptance criteria for the test to pass.

## RFFT Pipeline

The RFFT wrapper processes recorded IR and accelerometer signals using the production fixed-point FFT implementation.

The generated spectra are compared with the corresponding Python reference implementation for both the IR and AccX channels.

Because the absolute spectrum magnitude differs due to fixed-point scaling, the comparison focuses on spectral shape and peak location rather than identical raw power values.

The validator checks:

| Metric | Requirement |
| :--- | :--- |
| Full-spectrum correlation | ≥ 0.95 |
| HR-band correlation | ≥ 0.98 |
| HR-band NRMSE | ≤ 10% |
| Dominant peak difference | ≤ 1 FFT bin |

Both tested spectra must satisfy all acceptance criteria for the test to pass.

## HR Candidate Pipeline

The HR candidate wrapper runs the production FFT and HR candidate extraction stages.

The resulting candidates are compared with candidates produced by the Python reference implementation.

Because the Python and C++ implementations use different peak-detection methods, an exact match between all candidates is not expected.

The validator compares the strongest candidate from each implementation with all candidates produced by the other implementation. The test passes if a corresponding peak is found in either direction within a tolerance of one FFT bin.

| Metric | Requirement |
| :--- | :--- |
| Strongest candidate match | Required in either direction |
| Maximum bin difference | ≤ 1 FFT bin |

The test fails if either implementation produces no valid candidates.

The comparison is based on FFT bin position rather than exact candidate power or ordering.

## HR Estimation Pipeline

The HR estimation wrapper runs the complete production heart-rate algorithm on recorded sensor data.

The resulting raw and smoothed HR estimates are compared with reference heart-rate measurements recorded using an ECG chest strap.

Only windows containing both a valid estimated HR and a valid reference HR are included in the accuracy calculation.

The current acceptance limits are:

| Metric | Requirement |
| :--- | :--- |
| Mean Absolute Error (MAE) | ≤ 5 BPM |
| Root Mean Square Error (RMSE) | ≤ 7 BPM |
| Estimates within ±5 BPM | ≥ 50% |

The criteria are evaluated independently for both the raw HR result and the smoothed HR result. Both must pass for the complete test to pass.

Bias, maximum error and the number of valid windows are also reported for analysis.

## Full Vitals Pipeline

The full vitals wrapper runs the production HR and SpO2 algorithms on recorded raw sensor data.

It reproduces the firmware rolling-window behavior using a **1024-sample processing window** updated with **256 new samples** at a time.

For each completed analysis window, the wrapper records values used to evaluate the complete processing pipeline, including:

- raw HR,
- smoothed HR,
- reference ECG HR,
- SpO2,
- IR and RED DC values,
- IR and RED AC power,
- selected FFT bin.

### HR Validation

Raw and smoothed HR are independently compared with the ECG reference.

| Metric | Requirement |
| :--- | :--- |
| MAE | ≤ 5 BPM |
| RMSE | ≤ 7 BPM |
| Estimates within ±5 BPM | ≥ 50% |

### SpO2 Validation

No external reference SpO2 measurement is used in this test, so the validator checks output validity and stability rather than measurement accuracy.

Only non-zero SpO2 results are treated as valid estimates.

| Metric | Requirement |
| :--- | :--- |
| Valid SpO2 range | 70–100% |
| Maximum consecutive change | ≤ 5 percentage points |
| Values outside valid range | None |
| Consecutive jumps > 5 points | None |

At least one valid SpO2 result must be produced.

The complete pipeline test passes only when both HR variants and the SpO2 validation satisfy their respective criteria.

## Purpose

The pipeline tests verify that production firmware code behaves correctly when multiple processing stages are connected together.

They also provide a reproducible way to compare the fixed-point C++ implementation with:

- the original Python reference algorithms,
- recorded ECG reference measurements,
- expected intermediate signal-processing values.