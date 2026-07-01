#include "BLE.h"

ServerCallbacks::ServerCallbacks(BleManager *manager) : _manager(manager) {}

void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo & connInfo) {
    _manager->isConnected = true;
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo & connInfo, int reason) {
    _manager->isConnected = false;
    NimBLEDevice::startAdvertising();
}

// configures the NimBLE stack, creates service, characteristic and starts broadcasting
bool BleManager::begin() {
    NimBLEDevice::init("Wear");
    NimBLEServer *pServer = NimBLEDevice::createServer();

    if (pServer == nullptr) {
        return false;
    }
    
    pServer->setCallbacks(new ServerCallbacks(this));
    NimBLEService *pService = pServer->createService("11197df9-7de7-4edd-87ca-5e589b3d2e3a");

    pCharacteristic = pService->createCharacteristic("3ba2a546-ab9f-4ff4-b7ba-4056bd6a2779", NIMBLE_PROPERTY::NOTIFY);

    pService->start();
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setMinInterval(800);
    pAdvertising->setMaxInterval(800);
    pAdvertising->addServiceUUID("11197df9-7de7-4edd-87ca-5e589b3d2e3a");
    pAdvertising->start();

    return true;
}

bool BleManager::getConnectionState() {
    return isConnected;
}

void BleManager::sendPackage(uint8_t HR, uint8_t SpO2, uint8_t batteryLife) {
    if (isConnected && pCharacteristic != nullptr) {
        uint8_t packet[3] = {HR, SpO2, batteryLife};
        pCharacteristic->setValue(packet, 3);
        pCharacteristic->notify();
    }
}
