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

int MAX30102::readRegister(uint8_t reg) {
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
bool MAX30102::begin() {
    if (readRegister(MAX30102_PART_ID_ADDRESS) != 0x15) // 0x15 - MAX30102 part id
        return false;

    writeRegister(MAX30102_MODE_CONFIGURATION, MAX30102_RESET);

    bool resetSucces = false;
    delay(10);

    unsigned long startTime = millis();
    while (millis() - startTime < 100)
    {
        if ((readRegister(MAX30102_MODE_CONFIGURATION) & MAX30102_RESET) == 0) {
            resetSucces = true;
            break;
        }
        
        delay(5);
    }

    if (!resetSucces) {
        return false;
    }

    setup();

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
    FifoConfiguration();

}

void MAX30102::clearFIFO() {
    writeRegister(MAX30102_FIFO_WRITE_POINTER, 0);
    writeRegister(MAX30102_FIFO_OVERFLOW_COUNTER, 0);
    writeRegister(MAX30102_FIFO_READ_POINTER, 0);
    DiodeData.head = 0;
    DiodeData.tail = 0;
}

void MAX30102::FifoConfiguration() {
    writeRegister(MAX30102_FIFO_CONFIGURATION, 0x10); // 0x10 FIFO rollvower ON
}

void MAX30102::shutDown() {
    writeRegister(MAX30102_MODE_CONFIGURATION, 0x80);
}

void MAX30102::wakeUp() {
    writeRegister(MAX30102_MODE_CONFIGURATION, 0x00);
}

// transfer raw Red and IR samples from the hardware FIFO to the local ring buffer
void MAX30102::readNewData() {
    uint8_t read_pointer = readRegister(MAX30102_FIFO_READ_POINTER);
    uint8_t write_pointer = readRegister(MAX30102_FIFO_WRITE_POINTER);

    if (read_pointer == write_pointer) {
        return;
    }

    int number_of_samples = 0;
    number_of_samples = write_pointer - read_pointer;
    if (number_of_samples < 0) 
        number_of_samples += 32;

    int data_to_read = number_of_samples * 2 * 3; // number_of_samples * number_of_diodes (Red, Ir) * number of bytes (3 for each diode)

    while (data_to_read > 0) {
        int to_get = data_to_read;

        if (to_get > I2C_BUFFER_LENGTH) {
            to_get = I2C_BUFFER_LENGTH - (I2C_BUFFER_LENGTH % (2 * 3)); 
            // if request exceeds buffer, trim to fit whole samples 
            // 2 * 3 - number_of_diodes (Red, Ir) * number of bytes (3 for each diode)
        }

        data_to_read -= to_get;

        Wire.beginTransmission(_i2caddr);
        Wire.write(MAX30102_FIFO_DATA_REGISTER);
        Wire.endTransmission(false);

        Wire.requestFrom(_i2caddr, (uint8_t) to_get);
        while (to_get > 0) {

            if (Wire.available() < 6) {
                return;
            }

            uint8_t buffer[6];
            int i = 0;

            for (int i=0; i<6; i++) 
                buffer[i] = Wire.read();
            
            to_get -= 6;

            uint32_t temp_red_data = (uint32_t)buffer[0] << 16 | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2];
            uint32_t temp_ir_data = (uint32_t)buffer[3] << 16 | (uint32_t)buffer[4] << 8 | (uint32_t)buffer[5];

            DiodeData.storageRed[DiodeData.head] = temp_red_data & 0x3FFFF; // Mask off unused upper bits (keep only 18-bit data)
            DiodeData.storageIr[DiodeData.head] = temp_ir_data & 0x3FFFF;
            DiodeData.head++;

            if (DiodeData.head == STORAGE_SIZE)
                DiodeData.head = 0;

            if (DiodeData.head == DiodeData.tail) {
                DiodeData.tail++;

                if (DiodeData.tail == STORAGE_SIZE)
                    DiodeData.tail = 0;
            }
        }
    }

}

// return total count of unread samples
uint16_t MAX30102::available() {
    int16_t number_of_samples = DiodeData.head - DiodeData.tail;
    if (number_of_samples < 0)
        number_of_samples += STORAGE_SIZE;

    return (uint16_t)number_of_samples;
}

MaxSample MAX30102::readSample() {
    MaxSample result = {0, 0};

    if (available() > 0) {
        result.Red = DiodeData.storageRed[DiodeData.tail];
        result.Ir = DiodeData.storageIr[DiodeData.tail];

        DiodeData.tail++;
        if (DiodeData.tail == STORAGE_SIZE)
            DiodeData.tail = 0;
    }

    return result;
}