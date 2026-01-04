#include "Arduino.h"
#include "Wire.h"
#include "max30102_driver.h"

MAX30102::MAX30102(void) {
    _i2caddr = MAX30102_I2C_ADDRESS;
}

void MAX30102::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t MAX30102::readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2caddr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_i2caddr, (uint8_t) 1);

    uint8_t value = 0;

    if (Wire.available()) 
        value = Wire.read();
    

    return value;
}

bool MAX30102::begin() {
    if (readRegister(MAX30102_PART_ID_ADDRESS) != 0x15) // 0x15 - MAX30102 part id
        return false;

    writeRegister(MAX30102_MODE_CONFIGURATION, MAX30102_RESET);

    unsigned long startTime = millis();
    while (millis() - startTime < 100)
    {
        if ((readRegister(MAX30102_MODE_CONFIGURATION) & MAX30102_RESET) == 0)
            break;
        
        delay(1);
    }

    return true;
}

void MAX30102::setup() {
    writeRegister(MAX30102_MODE_CONFIGURATION, 0x03); // 0x03 - multi diode RED and IR

                                                      // Combined value 0x27 consists of:
    writeRegister(MAX30102_SPO2_CONFIGURATION, 0x27); // ADC Range: 4096nA 
                                                      // Sample Rate: 100Hz 
                                                      // Pulse Width: 411us
                                                
    writeRegister(MAX30102_LED1_PA, 0x1F); // LED Red Current 0x1F - 6.2 mA 
    writeRegister(MAX30102_LED2_PA, 0x1F); // LED IR Current 0x1F - 6.2 mA

    clearFIFO();

}

void MAX30102::clearFIFO() {
    writeRegister(MAX30102_FIFO_WRITE_POINTER, 0);
    writeRegister(MAX30102_FIFO_OVERFLOW_COUNTER, 0);
    writeRegister(MAX30102_FIFO_READ_POINTER, 0);
}

void MAX30102::readNewData() {
    uint8_t read_pointer = MAX30102::readRegister(MAX30102_FIFO_READ_POINTER);
    uint8_t write_pointer = MAX30102::readRegister(MAX30102_FIFO_WRITE_POINTER);

    if (read_pointer == write_pointer) {
        return;
    }

    int number_of_samples = 0;
    number_of_samples = write_pointer - read_pointer;
    if (number_of_samples < 0) 
        number_of_samples += 32;

    int data_to_read = number_of_samples * 2 * 3; // number_of_samples * number_of_diodes (Red, Ir) * number of bytes (3 for each diode)

    if (number_of_samples > 0) {
        Wire.beginTransmission(_i2caddr);
        Wire.write(MAX30102_FIFO_DATA_REGISTER);
        Wire.endTransmission(false);

        Wire.requestFrom(_i2caddr, (uint8_t) 6);
        uint8_t buffer[6];
        int i = 0;

        while (Wire.available() && i < 6) {
            buffer[i++] = Wire.read();
        }

        uint32_t temp_red_data = (uint32_t)buffer[0] << 16 | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2];
        uint32_t temp_ir_data = (uint32_t)buffer[3] << 16 | (uint32_t)buffer[4] << 8 | (uint32_t)buffer[5];

        DiodeData.Red = temp_red_data & 0x3FFFF; // Mask off unused upper bits (keep only 18-bit data)
        DiodeData.Ir = temp_ir_data & 0x3FFFF;

    }


}