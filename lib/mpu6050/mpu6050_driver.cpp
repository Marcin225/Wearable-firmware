#include "Arduino.h"
#include "Wire.h"
#include "mpu6050_driver.h"

// NOTE:
// This "MPU6050" module returns WHO_AM_I = 0x72 and appears to be
// an MPU-6500 or MPU-6500-compatible clone
// MPU-6500 register definitions are therefore used where required
// especially for Wake-on-Motion and low-power accelerometer operation

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
    if (Wire.endTransmission(false) != 0) {
        return -1;
    }

    int n = Wire.requestFrom(_i2caddr, (uint8_t) 1);
    if (n != 1) {
        return -1;
    }

    uint8_t value = 0;

    if (Wire.available()) {
        return value = Wire.read();
    }

    return -1;
}

// verify hardware identity and initialize sensor settings begin() -> setup()
bool MPU6050::begin() {
    // this MPU-6500-compatible clone returns 0x72
    // a genuine MPU-6050 normally returns 0x68
    // a genuine MPU-6500 normally returns 0x70
    if (readRegister(MPU6050_WHO_AM_I) != MPU6050_WHO_AM_I_ANSWER)
        return false;   

    writeRegister(MPU6050_PWR_MGMT_1, 0x80); // 0x80 - Reset

    delay(100);

    setup();

    return true;
}

void MPU6050::setup() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x08); // Wakes up the MPU6050 and disable temperature sensor (0x08) 
            //and sets the clock source to the 20 MHz oscillator (0x00) (gyroscope is in standby mode)

    writeRegister(MPU6050_PWR_MGMT_2, 0x07); // disable gyroscope axes

    // disable Wake-on-Motion logic
    writeRegister(MPU6050_INT_ENABLE, 0x00);
    writeRegister(MPU6500_ACCEL_INTEL_CTRL, 0x00);

    writeRegister(MPU6050_SMPLRT_DIV, 0x09); // 0x09 - accelerometer sample rate = 1 kHz / (1 + 9) = 100 Hz

    writeRegister(MPU6500_ACCEL_CONFIG2, 0x03); // accelerometer DLPF bandwidth = 41 Hz, internal output rate = 1 kHz
    // writeRegister(MPU6050_CONFIG, 0x03);

    writeRegister(MPU6050_ACCEL_CONFIG, 0x00); // 0x00 - full scale range 2g

    writeRegister(MPU6050_FIFO_EN, 0x00); //stop sampling to fifo

    writeRegister(MPU6050_USER_CTRL, 0x04); // reset FIFO

    writeRegister(MPU6050_USER_CTRL, 0x40); // enable FIFO

    writeRegister(MPU6050_FIFO_EN, 0x08); // enable FIFO for accelerometer data only

    mpuData.head = 0;
    mpuData.tail = 0;
}

void MPU6050::enableWakeOnMotion() {
    // NOTE:
    // Wake-on-Motion configuration for the MPU-6500-compatible clone

    // stop FIFO - not needed during Wake-on-Motion
    writeRegister(MPU6050_FIFO_EN, 0x00);
    writeRegister(MPU6050_USER_CTRL, 0x00);

    writeRegister(MPU6500_ACCEL_CONFIG2, 0x01); // 0x01 -> enable accel DLPF, A_DLPF_CFG = 1 (184 Hz bandwidth), recommended for WOM

    // 0x26 -> 152 mg WOM threshold (1 LSB = 4 mg)
    writeRegister(MPU6500_WOM_THR, 0x26); // ONE WOM threshold register shared by X/Y/Z

    // 0x80 -> INT_LEVEL – interrupt pin active low
    // 0x20 -> LATCH_INT_EN - keep interrupt asserted until status is cleared
    writeRegister(MPU6050_INT_PIN_CFG, 0xA0);

    // 0x80 -> ACCEL_INTEL_EN - This bit enables the Wake-on-Motion detection logic
    // 0x40 -> ACCEL_INTEL_MODE - Compare the current sample with the previous sample
    writeRegister(MPU6500_ACCEL_INTEL_CTRL, 0xC0);

    readRegister(MPU6050_INT_STATUS); // clear old interrupt

    writeRegister(MPU6050_INT_ENABLE, 0x40); // 0x40 -> INT_ENABLE - Enable WoM interrupt on accelerometer

    // low-power accelerometer sampling rate
    writeRegister(MPU6500_LP_ACCEL_ODR, 0x07); // 0x07 - Sample Rate (31.25 hz)

    // 0x20 -> ACCEL_CYCLE - enable low-power accelerometer cycle mode
    //      when SLEEP = 0 and accel axes are enabled, the chip alternates
    //      between sleep and taking a single accelerometer sample
    //      at a rate configured by LP_ACCEL_ODR
    // 0x08 -> TEMP_DIS - disable temperature sensor
    writeRegister(MPU6050_PWR_MGMT_1, 0x28); 

}

// clear interrupt flag
void MPU6050::getISRStatus() {
    readRegister(MPU6050_INT_STATUS);
}

void MPU6050::sleep() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x00); // stop accel samples to FIFO
    writeRegister(MPU6050_USER_CTRL, 0x04); // reset hardware FIFO
    mpuData.head = 0;
    mpuData.tail = 0;

    writeRegister(MPU6050_PWR_MGMT_1, 0x48); // SLEEP + TEMP_DIS
}

void MPU6050::wakeUp() {
    // wake up sensor, disable temperature sensor and use the internal 20 MHz oscillator as the clock source
    writeRegister(MPU6050_PWR_MGMT_1, 0x08);

    writeRegister(MPU6050_USER_CTRL, 0x04); // reset FIFO
    writeRegister(MPU6050_USER_CTRL, 0x40); // enable FIFO
    writeRegister(MPU6050_FIFO_EN, 0x08); // enable FIFO for accelerometer data only

    mpuData.head = 0;
    mpuData.tail = 0;
}

uint16_t MPU6050::getFifoCount() {
    Wire.beginTransmission(_i2caddr);
    Wire.write(MPU6050_FIFO_COUNT_H);
    Wire.endTransmission();

    if (Wire.requestFrom(_i2caddr, (uint8_t)2) != 2) {
        return 0;
    }

    uint16_t count = (uint16_t)Wire.read() << 8;
    count |= Wire.read();

    return count;
}

// read three-axis accelerometer samples from the hardware FIFO into the local ring buffer
void MPU6050::readNewData() {
    uint16_t fifoCount = getFifoCount();
    uint16_t samples = fifoCount / 6; // 3-axis accel | 1 axis = 2 bytes

    uint16_t data_to_read = samples * 6;

    while (data_to_read > 0) {
        int to_get = data_to_read;

        if (to_get > I2C_BUFFER_LENGTH) {
            to_get = I2C_BUFFER_LENGTH - (I2C_BUFFER_LENGTH % 6); 
            // if request exceeds buffer, trim to fit whole samples 
        }

        data_to_read -= to_get;

        Wire.beginTransmission(_i2caddr);
        Wire.write(MPU6050_FIFO_R_W);
        Wire.endTransmission(false);

        int received = Wire.requestFrom(_i2caddr, (uint8_t)to_get);
        if (received != to_get) {
            return;
        }

        while (to_get > 0 && Wire.available() >= 6) {
            uint8_t buffer[6];

            for (int i = 0; i < 6; i++) {
                buffer[i] = Wire.read();
            }

            to_get -= 6;

            MpuSample &currentSample = mpuData.StorageData[mpuData.head];

            currentSample.accX = (int16_t) ((buffer[0] << 8) | buffer[1]);
            currentSample.accY = (int16_t) ((buffer[2] << 8) | buffer[3]);
            currentSample.accZ = (int16_t) ((buffer[4] << 8) | buffer[5]);

            mpuData.head++;

            if (mpuData.head == STORAGE_SIZE) {
                mpuData.head = 0;
            }

            if (mpuData.head == mpuData.tail) {
                mpuData.tail++;

                if (mpuData.tail == STORAGE_SIZE) {
                    mpuData.tail = 0;
                }
            }
        }
    }
}

// return total count of unread samples
uint16_t MPU6050::available() {
    int16_t number_of_samples = mpuData.head - mpuData.tail;
    if (number_of_samples < 0)
        number_of_samples += STORAGE_SIZE;

    return (uint16_t)number_of_samples;
}

MpuSample MPU6050::readSample() {

    MpuSample result = {0,0,0};

    if (available() > 0) {
        result = mpuData.StorageData[mpuData.tail];
        mpuData.tail++;
        if (mpuData.tail == STORAGE_SIZE)
            mpuData.tail = 0;
    }

    return result;
}