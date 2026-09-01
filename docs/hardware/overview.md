# Hardware Overview

The prototype is built around the **Seeed Studio XIAO ESP32-C3** and combines optical sensing, motion sensing, battery monitoring and wireless communication in a compact wearable system.

The main hardware components are:

| Component | Role |
| :--- | :--- |
| **Seeed Studio XIAO ESP32-C3** | Main microcontroller, FreeRTOS processing and BLE communication |
| **MAX30102** | Optical PPG sensor for heart-rate and SpO2 measurements |
| **MPU6050-compatible IMU** | Accelerometer used for motion artifact detection and motion wake-up |
| **MAX17048** | Battery fuel gauge used to monitor Li-Po battery level |
| **3.7 V 400 mAh Li-Po battery** | Main power source |
| **Power switch** | Physically turns the device on and off |

## Main Controller

The **Seeed Studio XIAO ESP32-C3** is the main controller of the device.

It is responsible for:

- communication with all sensors,
- PPG and accelerometer data acquisition,
- signal preprocessing,
- heart-rate and SpO2 estimation,
- power-management logic,
- Bluetooth Low Energy communication.

The firmware runs on the ESP32-C3, utilizing the Arduino framework via PlatformIO and FreeRTOS.

## Optical Sensor

The **MAX30102** provides infrared and red-light PPG signals.

The infrared channel is used for heart-rate estimation and finger detection, while both infrared and red channels are used for SpO2 estimation.

The sensor FIFO allows samples to be collected in batches instead of requiring the microcontroller to read every sample individually.

## Motion Sensor

The prototype uses an **MPU6050-compatible IMU module**.

Although the module is sold as an MPU6050 clone, its register behavior appears to match the **ICM-20689** more closely. The actual device may therefore be an ICM-20689 or another compatible implementation.

Only the accelerometer is used by the firmware. The gyroscope remains disabled to reduce power consumption.

Accelerometer data is used for:

- motion artifact estimation during PPG processing,
- detecting device movement,
- waking the ESP32-C3 from deep sleep.

## Battery Monitoring

A **MAX17048** fuel-gauge IC is used to monitor the Li-Po battery.

It provides battery state information to the ESP32-C3 without requiring continuous battery-voltage estimation in software.

## Power Supply

The device is powered by a **3.7 V 400 mAh Li-Po battery**.

Power consumption is reduced through:

- ESP32-C3 deep sleep,
- MAX30102 shutdown mode,
- IMU low-power motion detection,
- disabled gyroscope,
- batch-based sensor acquisition.

A physical **ON/OFF switch** is included to completely disconnect or enable device power when required.

## Hardware Communication

The MAX30102, motion sensor and MAX17048 communicate with the ESP32-C3 through the I2C bus.

The MAX30102 additionally uses an interrupt line to notify the ESP32-C3 when a new FIFO batch is ready.

The motion sensor uses a separate interrupt line as a wake-up source when the ESP32-C3 is in deep sleep.