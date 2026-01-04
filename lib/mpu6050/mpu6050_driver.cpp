#include "Arduino.h"
#include "Wire.h"
#include "mpu6050_driver.h"

MPU6050::MPU6050(void) {
    _i2caddr = MPU6050_I2C_ADDRESS;
}

void MPU6050::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t MPU6050::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission(false);

    Wire.requestFrom(_i2caddr, (uint8_t) 1);
    uint8_t value = 0;

    if (Wire.available()) {
        value = Wire.read();
    }

    return value;
}

bool MPU6050::begin() {
    if (readRegister(MPU6050_WHO_AM_I) != MPU6050_DEVICE_ID)
        return false;

    writeRegister(MPU6050_PWR_MGMT_1, 0x80); // 0x80 - Reset

    unsigned long startTime = millis();
    while (millis() - startTime < 100)
    {
        if ((readRegister(MPU6050_PWR_MGMT_1) & 0x80) == 0)
            break;
        
        delay(1);
    }

    writeRegister(MPU6050_PWR_MGMT_1, 0x01); // Wakes up the MPU6050 
            //and sets the clock source to the X-axis gyroscope PLL instead of the internal 8 MHz oscillator
    delay(10);

    return true;
}

void MPU6050::setup() {
    writeRegister(MPU6050_CONFIG, 0x03); // 0x03 - 44 Hz Bandwidth

    writeRegister(MPU6050_GYRO_CONFIG, 0x08); // 0x08 - 500 degrees / s

    writeRegister(MPU6050_ACCEL_CONFIG, 0x00); // 0x00 - full scale range 2g
}

void MPU6050::readNewData() {
    Wire.beginTransmission(_i2caddr);
    Wire.write(MPU6050_ACCEL_XOUT_H);
    Wire.endTransmission(false);

    Wire.requestFrom(_i2caddr, (uint8_t) 14);

    uint8_t buffer[14];
    int i = 0;

    while (Wire.available() && i < 14) {
        buffer[i++] = Wire.read();
    }

    mpuData.accX = (int16_t) ((buffer[0] << 8) | buffer[1]);
    mpuData.accY = (int16_t) ((buffer[2] << 8) | buffer[3]);
    mpuData.accZ = (int16_t) ((buffer[4] << 8) | buffer[5]);

    int16_t raw_temp = (int16_t) ((buffer[6] << 8) | buffer[7]);
    mpuData.temp = (raw_temp / 340.0) + 36.53; // in degrees C

    mpuData.gyroX = (int16_t) ((buffer[8] << 8) | buffer[9]);
    mpuData.gyroY = (int16_t) ((buffer[10] << 8) | buffer[11]);
    mpuData.gyroZ = (int16_t) ((buffer[12] << 8) | buffer[13]);

}
