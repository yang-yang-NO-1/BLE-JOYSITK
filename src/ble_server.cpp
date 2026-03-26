#include "ble_server.h"
#include "config.h"

/**
 * @brief Handle BLE central connection events.
 */
void BleJoystickServer::ServerCallbacks::onConnect(NimBLEServer* /*server*/, NimBLEConnInfo& connInfo) {
    owner.deviceConnected = true;
    Serial.print("[BLE] Client connected: ");
    Serial.println(connInfo.getAddress().toString().c_str());
}

/**
 * @brief Handle BLE central disconnection events and restart advertising.
 */
void BleJoystickServer::ServerCallbacks::onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& connInfo, int reason) {
    owner.deviceConnected = false;
    Serial.print("[BLE] Client disconnected: ");
    Serial.print(connInfo.getAddress().toString().c_str());
    Serial.print(" reason=");
    Serial.println(reason);

    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising restarted");
}

/**
 * @brief Initialize BLE stack, server, service, characteristic and advertising.
 */
void BleJoystickServer::begin() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    Serial.print("[BLE] Local MAC: ");
    Serial.println(NimBLEDevice::getAddress().toString().c_str());

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks(*this));

    NimBLEService* service = server->createService(SERVICE_UUID);
    characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    uint8_t initPacket[5] = {0, 0, 0, 0, 0};
    characteristic->setValue(initPacket, sizeof(initPacket));

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();

    Serial.println("[BLE] Advertising started");
}

/**
 * @brief Notify a 5-byte joystick packet to the connected central.
 */
void BleJoystickServer::notifyState(const InputState& state) {
    if (!deviceConnected || characteristic == nullptr) {
        return;
    }

    uint8_t buttons = 0;
    if (state.joySwitchPressed) buttons |= (1U << 0);
    if (state.btn1Pressed)      buttons |= (1U << 1);
    if (state.btn2Pressed)      buttons |= (1U << 2);

    uint8_t packet[5];
    packet[0] = static_cast<uint8_t>(state.outX & 0xFF);
    packet[1] = static_cast<uint8_t>((state.outX >> 8) & 0xFF);
    packet[2] = static_cast<uint8_t>(state.outY & 0xFF);
    packet[3] = static_cast<uint8_t>((state.outY >> 8) & 0xFF);
    packet[4] = buttons;

    characteristic->setValue(packet, sizeof(packet));
    characteristic->notify();
}

/**
 * @brief Report current connection state.
 */
bool BleJoystickServer::isConnected() const {
    return deviceConnected;
}
