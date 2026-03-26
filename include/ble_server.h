#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "input_manager.h"

/**
 * @brief Simple BLE GATT server for joystick state notification.
 *
 * This class exposes a single custom service and a single notify/read
 * characteristic. The data payload is fixed at 5 bytes:
 *
 * byte0~1: X axis, little-endian uint16_t
 * byte2~3: Y axis, little-endian uint16_t
 * byte4  : button bitmap
 */
class BleJoystickServer {
public:
    /**
     * @brief Initialize BLE, create GATT server and start advertising.
     */
    void begin();

    /**
     * @brief Send the current input state to the central device if connected.
     *
     * @param state Current normalized input state.
     */
    void notifyState(const InputState& state);

    /**
     * @brief Query whether a BLE central device is currently connected.
     *
     * @return true if a central is connected, otherwise false.
     */
    bool isConnected() const;

private:
    NimBLECharacteristic* characteristic = nullptr;
    bool deviceConnected = false;

    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        explicit ServerCallbacks(BleJoystickServer& ownerRef) : owner(ownerRef) {}

        void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override;

    private:
        BleJoystickServer& owner;
    };
};

#endif
