#include "Arduino.h"

// NOTE:
// This "MPU6050" module returns WHO_AM_I = 0x98 and appears to be
// an ICM-20689 or ICM-20689-compatible clone
// Wake-on-Motion is therefore configured using ICM-20689 registers

#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#define STORAGE_SIZE                256

#define I2C_BUFFER_LENGTH           128 // esp32 c3 i2c buffer length

#define MPU6050_I2C_ADDRESS         0x68

#define MPU6050_DEVICE_ID           0x68
#define MPU6050_WHO_AM_I            0x75 
#define MPU6050_WHO_AM_I_ANSWER     0x98 // my mpu6050 clone return 0x98 as the WHO_AM_I response, a standard mpu6050 returns 0x68

#define MPU6050_MOT_THR             0x1F
#define MPU6050_MOT_DUR             0x20
#define MPU6050_INT_PIN_CFG         0x37
#define MPU6050_INT_ENABLE          0x38
#define MPU6050_MOT_DETECT_CTRL     0x69

#define MPU6050_CONFIG              0x1A
#define MPU6050_GYRO_CONFIG         0x1B
#define MPU6050_ACCEL_CONFIG        0x1C
#define MPU6050_SMPLRT_DIV          0x19

#define MPU6050_PWR_MGMT_1          0x6B
#define MPU6050_PWR_MGMT_2          0x6C
#define MPU6050_ACCEL_XOUT_H        0x3B

#define MPU6050_FIFO_COUNT_H        0x72
#define MPU6050_FIFO_COUNT_L        0x73
#define MPU6050_FIFO_R_W            0x74

#define MPU6050_FIFO_EN             0x23
#define MPU6050_USER_CTRL           0x6A


#define ICM20689_ACCEL_CONFIG2      0x1D
#define ICM20689_WOM_X_THR          0x20
#define ICM20689_WOM_Y_THR          0x21
#define ICM20689_WOM_Z_THR          0x22
#define ICM20689_INT_STATUS         0x3A
#define ICM20689_ACCEL_INTEL_CTRL   0x69
#define ICM20689_ACCEL_INTEL_CTRL   0x69


struct MpuSample {
    int16_t accX;
    int16_t accY;
    int16_t accZ;
};


class MPU6050 {
    public:
        MPU6050(void);

        bool begin();
        void setup();
        void readNewData();
        void sleep();
        void wakeUp();
        void enableWakeOnMotion();
        void getISRStatus();
        uint16_t available();
        MpuSample readSample();

    private:
        uint8_t _i2caddr;
        
        void writeRegister(uint8_t reg, uint8_t value);
        int readRegister(uint8_t reg);
        uint16_t getFifoCount();

        struct MPU6050_SensorData {
            MpuSample StorageData[STORAGE_SIZE];
            uint16_t head = 0;
            uint16_t tail = 0;
        };

        MPU6050_SensorData mpuData;
};

#endif