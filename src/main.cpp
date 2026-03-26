#include <Arduino.h>
#include "input_manager.h"
#include "ble_server.h"

static InputManager g_input;
static BleJoystickServer g_ble;

/**
 * @brief Arduino setup entry.
 *
 * Initializes serial output, input subsystem and BLE subsystem.
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("==== ESP32 BLE Joystick ====");

    g_input.begin();
    g_ble.begin();

    Serial.println("[SYS] Waiting for BLE client...");
    Serial.println("[SYS] Hold BTN1 + BTN2 for 2 seconds to recalibrate and save.");
}

/**
 * @brief Arduino loop entry.
 *
 * Samples the joystick/buttons, handles manual recalibration, notifies the BLE
 * central if connected, and prints a diagnostic line to the serial port.
 */
void loop() {
    InputState state = g_input.readState();

    if (g_input.shouldRecalibrate(millis(), state.btn1Pressed, state.btn2Pressed)) {
        Serial.println("[CAL] Recalibration triggered by BTN1 + BTN2");
        g_input.recalibrate();
        return;
    }

    g_ble.notifyState(state);

    Serial.printf(
        "RAW_X=%4u RAW_Y=%4u OUT_X=%4u OUT_Y=%4u SW=%d BTN1=%d BTN2=%d BLE=%d\n",
        state.rawX,
        state.rawY,
        state.outX,
        state.outY,
        state.joySwitchPressed ? 1 : 0,
        state.btn1Pressed ? 1 : 0,
        state.btn2Pressed ? 1 : 0,
        g_ble.isConnected() ? 1 : 0
    );

    delay(20);
}
