#ifndef MAX30102_DRIVER_H
#define MAX30102_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

#define STORAGE_SIZE                    256

#define I2C_BUFFER_LENGTH               128 // esp32 c3 i2c buffer length

#define MAX30102_PART_ID_ADDRESS        0xFF
#define MAX30102_RESET                  0x40

#define MAX30102_I2C_ADDRESS            0x57 // 7 bit

#define MAX30102_INTERRUPT_STATUS_1     0x00
#define MAX30102_INTERRUPT_ENABLE_1     0x02

#define MAX30102_MODE_CONFIGURATION     0x09
#define MAX30102_SPO2_CONFIGURATION     0x0A

#define MAX30102_FIFO_WRITE_POINTER     0x04
#define MAX30102_FIFO_OVERFLOW_COUNTER  0x05
#define MAX30102_FIFO_READ_POINTER      0x06
#define MAX30102_FIFO_DATA_REGISTER     0x07
#define MAX30102_FIFO_CONFIGURATION     0x08

#define MAX30102_LED1_PA                0x0C
#define MAX30102_LED2_PA                0x0D

struct MaxSample {
    uint32_t Red;
    uint32_t Ir;
};


class MAX30102 {
    public:
        MAX30102(void);

        bool begin();
        void setup();
        void clearFIFO();
        void readNewData();
        void shutDown();
        void wakeUp();
        void FifoConfiguration();

        uint16_t available();
        MaxSample readSample();

    
    private:
        uint8_t _i2caddr;

        void writeRegister(uint8_t reg, uint8_t value);
        uint8_t readRegister(uint8_t reg);

        struct SensorData {
            uint32_t storageRed[STORAGE_SIZE];
            uint32_t storageIr[STORAGE_SIZE];
            uint16_t head = 0;
            uint16_t tail = 0;
        };

        SensorData DiodeData;

};


#endif
