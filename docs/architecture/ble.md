# BLE Communication

The device uses Bluetooth Low Energy to transmit calculated vital-sign results to an external client.

BLE communication is implemented using the NimBLE-Arduino library and is managed by the `BleManager` module.

## BLE Configuration

| Parameter | Value |
| :--- | :--- |
| Device name | `Wear` |
| Service UUID | `11197df9-7de7-4edd-87ca-5e589b3d2e3a` |
| Characteristic UUID | `3ba2a546-ab9f-4ff4-b7ba-4056bd6a2779` |
| Characteristic type | Notify |
| Packet size | 3 bytes |

The BLE stack is initialized during firmware startup.

The device creates a single BLE service containing one notification characteristic and starts advertising the service UUID.

## Connection Handling

Connection state is tracked using an atomic boolean flag.

When a client connects, the flag is set to indicate an active BLE connection.

When the client disconnects:

1. the connection state is cleared,
2. BLE advertising is restarted automatically.

This allows another client to connect without restarting the device.

## Data Packet

Calculated results are transmitted as a compact three-byte packet:

| Byte | Data | Type |
| :--- | :--- | :--- |
| 0 | Heart rate | `uint8_t` |
| 1 | SpO2 | `uint8_t` |
| 2 | Battery level | `uint8_t` |

The packet is sent using a BLE notification:

```cpp
uint8_t packet[3] = {HR, SpO2, batteryLife};
pCharacteristic->setValue(packet, 3);
pCharacteristic->notify();
```

Data is transmitted only when a BLE client is connected and the notification characteristic has been initialized.

## Invalid Measurement Handling

A value of `0` for heart rate or SpO2 indicates that the corresponding measurement is currently considered unreliable.

The receiving application does not display `0` as a physiological value. Instead, unreliable HR or SpO2 measurements are presented as:

```text
--
```

This prevents invalid measurements from being interpreted as real vital-sign values.

## Data Flow

```mermaid
flowchart LR
    CALC[Vital-sign calculation]
    PACKET[3-byte BLE packet]
    CHAR[BLE Notify characteristic]
    CLIENT[Connected BLE client]

    CALC --> PACKET
    PACKET --> CHAR
    CHAR --> CLIENT
```

The calculation task provides the latest heart rate, SpO2 and battery percentage to the BLE manager, which packs the values and sends them to the connected client.

## Advertising

The BLE manager starts advertising after initialization and automatically resumes advertising after a client disconnects.

The advertised service UUID allows compatible clients to identify the measurement service before establishing a connection.