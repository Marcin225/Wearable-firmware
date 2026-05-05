
# Wearable Vital Signs Monitor  
  
A **wearable device prototype** for measuring vital signs and user activity, with planned mobile app integration.  
  
The device measures:  
  
- heart rate / HR,  
- blood oxygen saturation / SpO₂,  
- planned: steps, signal quality, and motion artefacts.  
  
The project runs on a **Seeed XIAO ESP32-C3**, uses **FreeRTOS**, **BLE** communication, and custom drivers and signal-processing algorithms.  
  
> The project is currently under development and will be updated regularly.  
  
---  
  
## Demo  
  
[Watch the demo video](https://www.youtube.com/shorts/DvwTR9ryYt4)  
    
---  
  
## Implemented  
  
- data reading from **MAX30102** and **MPU6050**,  
- custom sensor drivers for **MAX30102** and **MPU6050**,  
- task handling with **FreeRTOS**,  
- data buffering,  
- PPG signal filtering,  
- heart rate calculation using:  
	- peak detection,  
	- autocorrelation,  
- SpO₂ calculation using:  
	- Ratio of Ratios,  
- simple **confidence score** based on result fusion,  
- basic **BLE server** based on NimBLE,  
- stable HR and SpO₂ readings at rest.  
  
---  
  
## In Progress  
  
- motion artefact detection,  
- motion noise reduction,  
- NLMS filter testing,  
- mobile app for data preview,  
- confidence score improvement,  
- step counter,  
- prototype optimization for longer battery life.  
  
---  
  
## Components Used  
  
- Seeed XIAO ESP32-C3  
- MAX30102  
- MPU6050  
- PlatformIO  
- Arduino Framework  
- FreeRTOS  
- NimBLE  
  
---  
  
## Circuit Diagram  
  
![Circuit diagram](docs/schematic.png)  
  
---

## Project structure

```txt
include/
├── config.h
└── SystemContext.h

lib/
├── algorithms/
│   ├── PpgProcessor.cpp
│   └── algorithm_NLMS.cpp
├── ble_manager/
│   └── BLE.cpp
├── max30102/
│   └── max30102_driver.cpp
└── mpu6050/
    └── mpu6050_driver.cpp

src/
├── main.cpp
├── tasks.cpp
└── tasks.h
```

---
## How to Run

1.  Clone the repository:

```
https://github.com/Marcin225/Werable-firmware
cd Werable-firmware
```

2.  Open the project in **VS Code + PlatformIO**.
3.  Connect the sensors to the I2C bus.
4.  Build the project:

```
pio run
```

5.  Upload the firmware:

```
pio run --target upload
```

6.  Open the serial monitor:

```
pio device monitor
```