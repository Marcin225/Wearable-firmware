#include "max17048_driver.h"

MAX17048::MAX17048(void) {
    _i2caddr = MAX17048_I2C_ADDRESS;
}

// the MAX17048 operates on 16-bit reads and writes
// the IC expects the Most Significant Byte (MSB) first followed by the Least Significant Byte (LSB)
void MAX17048::writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8)); // MSB
    Wire.write((uint8_t)(value & 0xFF)); // LSB
    Wire.endTransmission();
}

int MAX17048::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return -1;
    }

    int n = Wire.requestFrom(_i2caddr, (uint8_t) 2);
    if (n != 2) {
        return -1;
    }

    uint16_t value =  0;
    value = Wire.read() << 8; // MSB
    value = value | Wire.read(); // LSB

    return value;
}

bool MAX17048::begin() {
    Wire.beginTransmission(MAX17048_I2C_ADDRESS);

    if (Wire.endTransmission() != 0)
        return false;

    writeRegister(MAX17048_CMD, MAX17048_RESET_CMD); // reset

    delay(10);

    return true;
}

int MAX17048::readBatteryPercent() {
    int socRaw = readRegister(MAX17048_SOC);

    if (socRaw == -1) {
        return -1;
    }

    // SOC register consists of MSB integer (0 - 100 %) and LSB fraction (1 / 256 %)
    // we only need the integer part 
    uint8_t batteryPercent = socRaw >> 8;

    return batteryPercent & 0xFF; // clear all bits above bit 7
}

void MAX17048::sleep() {
    int config = readRegister(MAX17048_CONFIG);

    if (config != -1) {

        // set bit 7 to start sleep mode
        config |= 0x0080;
        writeRegister(MAX17048_CONFIG, config);
    }
}

void MAX17048::wakeUp() {
    int config = readRegister(MAX17048_CONFIG);

    if (config != -1) {

        // clear bit 7 to resume normal operation
        config &= ~0x0080;
        writeRegister(MAX17048_CONFIG, config);
    }
}