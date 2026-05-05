#include "Arduino.h"

#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#define STORAGE_SIZE                256

#define I2C_BUFFER_LENGTH           128 // esp32 c3 i2c buffer length

#define MPU6050_I2C_ADDRESS         0x68

#define MPU6050_DEVICE_ID           0x68
#define MPU6050_WHO_AM_I            0x98 // my mpu6050 clone have 0x98, normally WHO_AM_I is 0x75

#define MPU6050_CONFIG              0x1A
#define MPU6050_GYRO_CONFIG         0x1B
#define MPU6050_ACCEL_CONFIG        0x1C
#define MPU6050_SMPLRT_DIV          0x19

#define MPU6050_PWR_MGMT_1          0x6B
#define MPU6050_ACCEL_XOUT_H        0x3B

struct MpuSample {
    int16_t accX;
    int16_t accY;
    int16_t accZ;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
};


class MPU6050 {
    public:
        MPU6050(void);

        bool begin();
        void setup();
        void readNewData();
        void sleep();
        void wakeUp();
        uint16_t available();
        
        MpuSample readSample();

    private:
        uint8_t _i2caddr;
        
        void writeRegister(uint8_t reg, uint8_t value);
        int readRegister(uint8_t reg);

        struct MPU6050_SensorData {
            MpuSample StorageData[STORAGE_SIZE];
            uint16_t head = 0;
            uint16_t tail = 0;
        };

        MPU6050_SensorData mpuData;
};

#endif