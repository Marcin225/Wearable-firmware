# System Architecture Overview

The firmware is developed in **PlatformIO** using the Arduino framework and FreeRTOS on the Seeed Studio XIAO ESP32-C3.

The ESP32-C3 acts as the main microcontroller and manages sensor acquisition, signal processing, power management, and Bluetooth Low Energy communication.

The system uses two main sensors:

- **MAX30102** – optical PPG sensor used to acquire infrared (IR) and red-light signals for heart-rate and SpO2 estimation.
- **MPU6050-compatible IMU** – used to measure acceleration for motion artifact reduction and to wake the device from deep sleep when motion is detected.

The IMU module used in the prototype is sold as an MPU6050 clone. However, its register behavior appears to match the ICM-20689 more closely, so the actual sensor may be an ICM-20689 or a compatible device.

The PPG signal is sampled at **100 Hz**. The MAX30102 FIFO is read in batches of approximately **28 samples every 280 ms**, reducing the number of sensor transactions and allowing the processor to remain idle between acquisitions.

## FreeRTOS Architecture

The firmware uses FreeRTOS to separate time-sensitive data acquisition from computationally heavier signal processing.

The application is divided into two main tasks:

### `vCollectAndFilterDataTask`

The collector task has higher priority and is responsible for:

- reading new MAX30102 samples after a FIFO interrupt,
- reading accelerometer samples from the IMU,
- aligning accelerometer data with the PPG samples,
- applying initial signal preprocessing,
- detecting finger presence,
- managing measurement sessions,
- handling the WORK, CHECK, and low-power operating states,
- passing filtered data to the calculation task.

Each signal channel is preprocessed using:

1. a 3-sample median filter,
2. a Butterworth band-pass filter with a passband of approximately **0.4–4 Hz**.

Filtered samples are collected into blocks of **256 samples** before being transferred to the calculation task.

### `vCalculateVitalsTask`

The calculation task processes the collected data and is responsible for:

- maintaining the 1024-sample analysis window,
- estimating heart rate,
- estimating SpO2,
- smoothing the displayed heart-rate value,
- checking the battery level,
- preparing and sending measurement results through BLE.

The full analysis window contains **1024 samples**, which corresponds to **10.24 seconds** of signal at 100 Hz.

New data is added in blocks of **256 samples**, so after the initial analysis window has been filled, a new result can be calculated approximately every **2.56 seconds**.

## Operating Modes

The firmware uses three main operating modes:

- **WORK** – normal measurement mode with continuous PPG and accelerometer acquisition.
- **CHECK** – low-power mode used after motion wake-up to periodically check for finger presence.
- **SLEEP** – the ESP32-C3 enters deep sleep, the MAX30102 is shut down, and the IMU remains active only for motion detection.

A motion interrupt from the IMU wakes the device and starts the CHECK mode. When a valid PPG signal is detected, the system returns to WORK mode.

## Bluetooth Low Energy

BLE communication is implemented using the **NimBLE-Arduino** library.

The ESP32-C3 operates as a BLE server and advertises the device for discovery by a client. New measurement results are transmitted approximately every **2.56 seconds**, matching the processing interval of the calculation task.

## Task Separation

Separating data acquisition and signal processing into independent FreeRTOS tasks keeps sensor collection responsive during more computationally intensive HR and SpO2 calculations.

It also allows the firmware to use task notifications, priorities, and low-power periods more effectively than a single sequential application loop.