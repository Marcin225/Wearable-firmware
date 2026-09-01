#ifndef MAX17048_DRIVER_H
#define MAX17048_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

#define MAX17048_I2C_ADDRESS            0x36 // 7 bit

#define MAX17048_SOC                    0x04

#define MAX17048_CONFIG                 0x0C

#define MAX17048_CMD                    0xFE
#define MAX17048_RESET_CMD              0x5400



class MAX17048 {
    public:
        MAX17048(void);

        bool begin();
        int readBatteryPercent();
        void sleep();
        void wakeUp();

    private:
        uint8_t _i2caddr;

        void writeRegister(uint8_t reg, uint16_t value);
        int readRegister(uint8_t reg);
};


#endif