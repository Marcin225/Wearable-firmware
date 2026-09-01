# Project Structure

The project is divided into firmware modules, application-level code, tests, analysis scripts, calibration tools and documentation.

```text
SmartBandProject/
├── analysis/
├── docs/
├── include/
├── lib/
│   ├── algorithms/
│   ├── ble_manager/
│   ├── max30102/
│   ├── max17048/
│   ├── measurement/
│   ├── mpu6050/
│   ├── ppg_processors/
│   └── utils/
├── src/
├── test/
├── tools/
├── platformio.ini
└── README.md
```

## `analysis/`

Contains Python implementations and offline analysis scripts used during algorithm development, comparison and validation. This includes reference implementations, validation scripts, processing experiments and plotting utilities.

## `docs/`

Contains the project documentation covering architecture, hardware, signal processing, testing, validation and development.

## `include/`

Contains shared configuration and structures used by multiple firmware modules.

## `lib/`

Contains the main reusable firmware modules.

- **`algorithms/`** – Contains low-level spectral algorithms, primarily the fixed-point real FFT implementation and selected-bin spectral calculations.
- **`ble_manager/`** – Contains the complete BLE communication layer based on NimBLE, including server initialization, service and characteristic configuration, advertising, connection-state handling and transmission of HR, SpO2 and battery data.
- **`max30102/`** – Contains the MAX30102 driver responsible for sensor configuration, FIFO handling, interrupt configuration, shutdown control and IR/RED sample acquisition.
- **`max17048/`** – Contains the MAX17048 battery fuel-gauge driver used to read the battery state of charge and provide the battery percentage reported by the firmware.
- **`measurement/`** – Contains measurement-related data structures, reusable measurement buffers and logic used to prepare and organize sensor data before vital-sign calculation.
- **`mpu6050/`** – Contains the inertial sensor driver responsible for accelerometer configuration, FIFO acquisition and motion-detection functionality. The driver keeps the MPU6050 name although the detected device behaves more like an ICM-20689-compatible sensor.
- **`ppg_processors/`** – Contains the main PPG processing pipeline, including signal preprocessing, HR candidate processing and estimation, SpO2 DC and AC processing, post-processing and result smoothing.
- **`utils/`** – Contains common integer helper functions and precomputed lookup tables used by the signal-processing modules, including FFT tables and mathematical utilities such as integer square root.

## `src/`

Contains application-level firmware code. This includes the main program entry point and the implementation of the FreeRTOS tasks responsible for sensor acquisition, measurement processing and vital-sign calculation.

## `test/`

Contains native C++ wrappers, Unity unit tests and Python validation scripts. The tests are organized around individual processing stages as well as the complete vital-sign pipeline.

## `tools/`

Contains tools used for algorithm calibration and parameter evaluation. Current tools include:

- **`calibrate_th_cf.cpp`** – evaluates different confidence-factor thresholds.
- **`calibrate_weights.cpp`** – evaluates combinations of motion-penalty and history-bonus weights.

The tools use the production signal-processing implementation with recorded datasets.

## `platformio.ini`

Contains the PlatformIO configuration used for firmware compilation and upload.