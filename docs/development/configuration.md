# Firmware Configuration

The main firmware parameters are defined in `include/config.h`.

The same configuration values are reused by the production firmware, tests and calibration tools where applicable, avoiding duplicated parameter definitions.

## Signal Processing

| Parameter | Value | Description |
| :--- | ---: | :--- |
| `SAMPLING_RATE_HZ` | 100 | Sampling frequency of the PPG and accelerometer signals |
| `BUFFER_SIZE` | 1024 | Number of samples in the main processing window |
| `CHUNK_SIZE` | 256 | Number of samples transferred between acquisition and calculation tasks |
| `FFT_SIZE` | 2048 | Effective FFT size after zero padding |
| `SPECTRUM_SIZE` | 1025 | Number of positive-frequency bins produced by the real FFT |

A new vital-sign calculation is performed after every 256 new samples once the initial 1024-sample window has been filled.

At a sampling rate of 100 Hz, this results in an update interval of approximately 2.56 seconds.

## Hardware Configuration

| Parameter | Value | Description |
| :--- | ---: | :--- |
| `I2C_SDA_PIN` | 6 | I2C SDA GPIO |
| `I2C_SCL_PIN` | 7 | I2C SCL GPIO |
| `FINGER_IR_THRESHOLD` | 50000 | Raw IR threshold used for finger detection |

The MAX30102, inertial sensor and MAX17048 share the same I2C bus.

## FreeRTOS Tasks

| Parameter | Value | Description |
| :--- | ---: | :--- |
| `TASK_DATA_STACK_SIZE` | 4096 | Stack size of the sensor acquisition task |
| `TASK_CALC_STACK_SIZE` | 8192 | Stack size of the vital-sign calculation task |
| `TASK_DATA_PRIORITY` | 2 | Priority of the acquisition task |
| `TASK_CALC_PRIORITY` | 1 | Priority of the calculation task |

The acquisition task has a higher priority because sensor data must be read before the hardware FIFOs overflow. The calculation task can therefore be temporarily preempted when a new sensor batch becomes available.

## Heart-Rate Estimation

| Parameter | Value | Description |
| :--- | ---: | :--- |
| `BONUS_Q12` | 61 | Weight of the history bonus used during HR candidate scoring |
| `MAIN_PENALTY_Q12` | 4506 | Weight of the motion penalty used during HR candidate scoring |
| `TH_CF_Q12` | 29491 | Confidence-factor threshold used by the HR state machine |

The scoring parameters are stored in Q12 fixed-point format.

Their approximate real values are:

| Parameter | Approximate value |
| :--- | ---: |
| `BONUS_Q12` | 0.015 |
| `MAIN_PENALTY_Q12` | 1.10 |
| `TH_CF_Q12` | 7.20 |

`BONUS_Q12` and `MAIN_PENALTY_Q12` affect HR candidate scoring, while `TH_CF_Q12` is used after candidate selection by the HR state machine to evaluate spectral peak quality.

The calibration procedure for these parameters is described in [parameter-tuning.md](../testing/parameter_tuning.md).

## Motion Detection

| Parameter | Value | Description |
| :--- | ---: | :--- |
| `MOTION_THRESHOLD` | 50000 | Motion threshold used by the power-management logic |

Motion information is used both for HR artifact suppression and for deciding whether the device should remain active, enter CHECK mode or return to deep sleep.

## Fixed-Point Configuration

The signal-processing pipeline uses integer and fixed-point arithmetic instead of floating-point calculations.

Different parts of the pipeline use different fixed-point formats depending on the required range and precision:

| Format | Main usage |
| :--- | :--- |
| Q2.30 | Butterworth filter coefficients |
| Q1.31 | FFT twiddle factors and normalized spectral values |
| Q14 | HR candidate frequencies |
| Q12 | HR scoring weights and confidence-factor threshold |
| Q8 | HR output smoothing coefficient |

Detailed implementation information is available in the signal-processing documentation.