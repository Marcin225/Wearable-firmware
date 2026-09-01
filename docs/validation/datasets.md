# Validation Datasets

The validation dataset contains recordings collected under different motion conditions to evaluate heart-rate estimation, motion artifact handling and overall signal-processing behavior.

During all recordings, the optical sensor was worn on the finger. A chest-worn ECG strap was used simultaneously as a reference for heart-rate measurements and ground-truth comparison.

The complete recordings and raw datasets are available here:

[Google Drive – validation recordings](https://drive.google.com/drive/folders/1DDzSgPqNOSl7SWRH2JdoCzcJggn3kJec?usp=sharing)

## Dataset Overview

| File | Duration | Initial rest | Activity |
| :--- | :---: | :---: | :--- |
| `01_rest.csv` | 5 min | — | Rest |
| `02_arm_linear_movements.csv` | 7 min | 45 s | Linear arm and finger movements |
| `03_arm_rotational_movements.csv` | 7 min | 45 s | Rotational arm and wrist movements |
| `04_marching_in_place.csv` | 7 min | 45 s | Marching and light jogging in place |
| `05_brisk_walk.csv` | 7 min | 1 min | Brisk treadmill walk at approximately 5 km/h |
| `06_jogging.csv` | 7 min | 1 min | Treadmill jogging at approximately 9 km/h |
| `07_random_motion.csv` | 7 min | Variable | Mixed random movements |

## Recording Conditions

### `01_rest.csv`

A **5-minute resting recording** with minimal intentional movement.

This dataset provides a baseline for evaluating HR estimation under low-motion conditions.

### `02_arm_linear_movements.csv`

A **7-minute recording** beginning with approximately **45 seconds of rest**.

The movement section includes mainly:

- moving the arm up and down,
- moving the arm sideways,
- similar movements performed mainly with the finger while the sensor remained attached to it.

This dataset introduces motion directly affecting the finger-mounted sensor.

### `03_arm_rotational_movements.csv`

A **7-minute recording** beginning with approximately **45 seconds of rest**.

The movement section consists mainly of rotational movements of the arm and wrist while attempting to keep the finger and sensor position relatively stable.

This dataset is intended to introduce rotational motion without intentionally moving the sensor finger itself.

### `04_marching_in_place.csv`

A **7-minute recording** beginning with approximately **45 seconds of rest**.

The active section contains movements performed in place, including:

- marching,
- light jogging,
- similar low-intensity whole-body movements.

### `05_brisk_walk.csv`

A **7-minute recording** beginning with approximately **1 minute of rest**.

The active section consists of brisk walking on a treadmill at approximately **5 km/h**.

### `06_jogging.csv`

A **7-minute recording** beginning with approximately **1 minute of rest**.

The active section consists of jogging on a treadmill at approximately **9 km/h**.

### `07_random_motion.csv`

A **7-minute mixed-motion** recording with irregular periods of activity and rest. The movement sequence was intentionally less structured and included:

- squats,
- body rotations,
- jumps,
- short dance-like movements,
- finger pressing,
- other random motion.

This dataset is intended to represent less predictable movement than the controlled recordings above.

## Purpose

The dataset covers progressively more difficult motion conditions:

```mermaid
flowchart TD
    REST[Rest] --> ARM[Controlled arm motion]
    ARM --> ROT[Rotational motion]
    ROT --> BODY[Whole-body movement]
    BODY --> WALK[Walking]
    WALK --> JOG[Jogging]
    JOG --> RAND[Random motion]