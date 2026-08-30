#include "Arduino.h"
#include "Wire.h"
#include "mpu6050_driver.h"

// NOTE:
// This "MPU6050" module returns WHO_AM_I = 0x98 and appears to be
// an ICM-20689 or ICM-20689-compatible clone.
// Wake-on-Motion is therefore configured using ICM-20689 registers.

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
    
    //my mpu6050 clone return 0x98 as the WHO_AM_I response, a standard mpu6050 returns 0x68
    if (readRegister(MPU6050_WHO_AM_I) != MPU6050_WHO_AM_I_ANSWER)
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

    delay(10);
    setup();

    return true;
}

void MPU6050::setup() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x08); // Wakes up the MPU6050 and disable temperature sensor (0x08) 
            //and sets the clock source to the 8 MHz oscillator (0x00) (gyroscope is in standby mode)

    writeRegister(MPU6050_PWR_MGMT_2, 0x07); // disable gyroscope axes

    // disable Wake-on-Motion logic
    writeRegister(MPU6050_INT_ENABLE, 0x00);
    writeRegister(ICM20689_ACCEL_INTEL_CTRL, 0x00);

    writeRegister(MPU6050_SMPLRT_DIV, 0x09); // 0x09 - Sample Rate (100 hz) = Gyroscope Output Rate / (1 + SMPLRT_DIV)

    writeRegister(ICM20689_ACCEL_CONFIG2, 0x03); // accelerometer DLPF bandwidth = 44.8 Hz, internal output rate = 1 kHz

    writeRegister(MPU6050_ACCEL_CONFIG, 0x00); // 0x00 - full scale range 2g

    writeRegister(MPU6050_FIFO_EN, 0x00); //stop sampling to fifo

    writeRegister(MPU6050_USER_CTRL, 0x04); // reset FIFO

    writeRegister(MPU6050_USER_CTRL, 0x40); // enable FIFO

    writeRegister(MPU6050_FIFO_EN, 0x08); // enable FIFO for accelerometer data only.

    mpuData.head = 0;
    mpuData.tail = 0;
}

void MPU6050::enableWakeOnMotion() {
    // NOTE:
    // This "MPU6050" module returns WHO_AM_I = 0x98 and appears to be
    // an ICM-20689 or ICM-20689-compatible clone.
    // Wake-on-Motion is therefore configured using ICM-20689 registers.

    writeRegister(ICM20689_ACCEL_CONFIG2, 0x01); // 0x01 -> enable accel DLPF, A_DLPF_CFG = 1 (218.1 Hz bandwidth), recommended for WOM

    // 0x4B -> 300 mg WOM threshold (1 LSB = 4 mg)
    writeRegister(ICM20689_WOM_X_THR, 0x4B); // WOM threshold X
    writeRegister(ICM20689_WOM_Y_THR, 0x4B); // WOM threshold Y
    writeRegister(ICM20689_WOM_Z_THR, 0x4B); //  WOM threshold Z

    // 0x80 -> INT_LEVEL 1 – The logic level for INT/DRDY pin is active low
    // 0x20 -> LATCH_INT_EN - INT/DRDY pin level held until interrupt status is cleared.
    writeRegister(MPU6050_INT_PIN_CFG, 0xA0);

    // 0x80 -> ACCEL_INTEL_EN - This bit enables the Wake-on-Motion detection logic
    // 0x40 -> ACCEL_INTEL_MODE - Compare the current sample with the previous sample
    writeRegister(ICM20689_ACCEL_INTEL_CTRL, 0xC0);

    readRegister(ICM20689_INT_STATUS); // clear old interrupt

    writeRegister(MPU6050_INT_ENABLE, 0xE0); // 0xE0 -> INT_ENABLE - Enable WoM interrupt on accelerometer

    writeRegister(MPU6050_SMPLRT_DIV, 0x13); // 0x09 -> SMPLRT_DIV - Sample Rate (50 hz) = SAMPLE_RATE = INTERNAL_SAMPLE_RATE / (1 + SMPLRT_DIV)

    // 0x20 -> ACCEL_CYCLE - enable low-power accelerometer cycle mode
    //      when SLEEP = 0 and accel axes are enabled, the chip alternates
    //      between sleep and taking a single accelerometer sample
    //      at a rate determined by SMPLRT_DIV
    // 0x08 -> TEMP_DIS - disable temperature sensor
    writeRegister(MPU6050_PWR_MGMT_1, 0x28); 

}

void MPU6050::getISRStatus() {
    readRegister(ICM20689_INT_STATUS);
}

void MPU6050::sleep() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x40); // 0x40 sleep ON
}

void MPU6050::wakeUp() {
    writeRegister(MPU6050_PWR_MGMT_1, 0x08); // Wakes up the MPU6050 and disable temperature sensor (0x08) 
            //and sets the clock source to the 8 MHz oscillator (0x00) (gyroscope is in standby mode)
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

// reads 6 bytes of accel and gyro data and push it into ring buffer
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