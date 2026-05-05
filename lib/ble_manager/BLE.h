#ifndef BLE_H
#define BLE_H

#include <NimBLEDevice.h>
#include <atomic>

class BleManager;

// event handler for tracking and responding to wireless connection changes
class ServerCallbacks: public NimBLEServerCallbacks {
    private:
        BleManager *_manager;

    public:
        ServerCallbacks(BleManager *manager);
        void onConnect(NimBLEServer *pServer, NimBLEConnInfo & connInfo) override;
        void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo & connInfo, int reason) override;
};

// interface for managing the BLE stack and data transmission
class BleManager {
    public:
        BleManager() = default;

        bool begin();
        bool getConnectionState();
        void sendPackage(uint8_t HR, uint8_t SpO2, uint8_t batteryLife);
    
    private:
        friend class ServerCallbacks;
        std::atomic<bool> isConnected{false}; // thread-safe flag, worth using although ESP32-C3 has only one core
        NimBLECharacteristic *pCharacteristic = nullptr;
};

#endif