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

int MPU6050::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(_i2caddr, (uint8_t) 1);
    uint8_t value = 0;

    if (Wire.available()) {
        return value = Wire.read();
    }

    return -1;
}

bool MPU6050::begin() {
    if (readRegister(MPU6050_WHO_AM_I) != MPU6050_DEVICE_ID)
        return false;

    writeRegister(MPU6050_PWR_MGMT_1, 0x80); // 0x80 - Reset

    bool resetSucces = false;
    delay(10);

    unsigned long startTime = millis();
    while (millis() - startTime < 100)
    {
        if ((readRegister(MPU6050_PWR_MGMT_1) & 0x80) == 0) {
            resetSucces = true;
            break;
        }
        
        delay(5);
    }

    if (!resetSucces) {
        return false;
    }

    writeRegister(MPU6050_PWR_MGMT_1, 0x01); // Wakes up the MPU6050 
            //and sets the clock source to the X-axis gyroscope PLL instead of the internal 8 MHz oscillator
    delay(10);
    setup();

    return true;
}

void MPU6050::setup() {
    writeRegister(MPU6050_SMPLRT_DIV, 0x13); // 0x13 - Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)

    writeRegister(MPU6050_CONFIG, 0x03); // 0x03 - 44 Hz Bandwidth

    writeRegister(MPU6050_GYRO_CONFIG, 0x08); // 0x08 - 500 degrees / s

    writeRegister(MPU6050_ACCEL_CONFIG, 0x00); // 0x00 - full scale range 2g


    mpuData.head = 0;
    mpuData.tail = 0;
}

void MPU6050::sleep() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x40); // 0x40 sleep ON
}

void MPU6050::wakeUp() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x01); // 0x01 sleep OFF and sets the clock source to the X-axis gyroscope
}

void MPU6050::readNewData() {
    Wire.beginTransmission(_i2caddr);
    Wire.write(MPU6050_ACCEL_XOUT_H);
    Wire.endTransmission();

    Wire.requestFrom(_i2caddr, (uint8_t) 14);

    if (Wire.available() < 14)
        return;

    uint8_t buffer[14];
    int i = 0;

    while (Wire.available() && i < 14) {
        buffer[i++] = Wire.read();
    }

    MpuSample &currentSample = mpuData.StorageData[mpuData.head];

    currentSample.accX = (int16_t) ((buffer[0] << 8) | buffer[1]);
    currentSample.accY = (int16_t) ((buffer[2] << 8) | buffer[3]);
    currentSample.accZ = (int16_t) ((buffer[4] << 8) | buffer[5]);

    int16_t raw_temp = (int16_t) ((buffer[6] << 8) | buffer[7]);
    currentSample.temp = (raw_temp / 340.0) + 36.53; // in degrees C

    currentSample.gyroX = (int16_t) ((buffer[8] << 8) | buffer[9]);
    currentSample.gyroY = (int16_t) ((buffer[10] << 8) | buffer[11]);
    currentSample.gyroZ = (int16_t) ((buffer[12] << 8) | buffer[13]);

    mpuData.head++;

    if (mpuData.head == STORAGE_SIZE)
        mpuData.head = 0;

    if (mpuData.head == mpuData.tail) {
        mpuData.tail++;

        if (mpuData.tail == STORAGE_SIZE)
            mpuData.tail = 0;
    }

}

uint16_t MPU6050::available() {
    int16_t number_of_samples = mpuData.head - mpuData.tail;
    if (number_of_samples < 0)
        number_of_samples += STORAGE_SIZE;

    return (uint16_t)number_of_samples;
}

MpuSample MPU6050::readSample() {

    MpuSample result = {0,0,0,0,0,0,0};

    if (available() > 0) {
        result = mpuData.StorageData[mpuData.tail];
        mpuData.tail++;
        if (mpuData.tail == STORAGE_SIZE)
            mpuData.tail = 0;
    }

    return result;
}