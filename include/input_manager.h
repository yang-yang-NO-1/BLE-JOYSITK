#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "calibration_store.h"

/**
 * @brief Normalized joystick data and button state.
 *
 * x and y are normalized to the range 0~4095, with 2048 representing the
 * calibrated center position. The three button fields are boolean states.
 */
struct InputState {
    uint16_t rawX;
    uint16_t rawY;
    uint16_t outX;
    uint16_t outY;
    bool joySwitchPressed;
    bool btn1Pressed;
    bool btn2Pressed;
};

/**
 * @brief Handles ADC sampling, joystick calibration, filtering and button scan.
 */
class InputManager {
public:
    /**
     * @brief Initialize GPIO/ADC and load or create calibration data.
     *
     * The manager first attempts to load persisted calibration data from NVS.
     * If no valid data is found, it runs a full center + outer-ring calibration
     * and stores the result automatically.
     */
    void begin();

    /**
     * @brief Read the current input state.
     *
     * @return InputState Latest normalized joystick and button state.
     */
    InputState readState();

    /**
     * @brief Check whether BTN1 and BTN2 have been held long enough to trigger recalibration.
     *
     * @param nowMs Current millis() timestamp.
     * @param btn1Pressed Current BTN1 state.
     * @param btn2Pressed Current BTN2 state.
     * @return true if recalibration should run now, otherwise false.
     */
    bool shouldRecalibrate(unsigned long nowMs, bool btn1Pressed, bool btn2Pressed);

    /**
     * @brief Execute center calibration followed by outer-ring calibration and save to NVS.
     */
    void recalibrate();

    /**
     * @brief Print the current calibration data to Serial.
     */
    void printCalibration() const;

    /**
     * @brief Export current calibration values.
     */
    CalibrationData getCalibrationData() const;

private:
    int centerX = 2048;
    int centerY = 2048;
    int minX = 0;
    int maxX = 4095;
    int minY = 0;
    int maxY = 4095;

    float filteredX = 0.0f;
    float filteredY = 0.0f;
    bool filterInitialized = false;
    unsigned long recalibPressStart = 0;
    CalibrationStore store;

    uint16_t readAdcAverage(uint8_t pin, int samples) const;
    int clampInt(int value, int lo, int hi) const;
    int mapAxisCalibrated(int raw, int minVal, int centerVal, int maxVal) const;
    void applyCalibrationData(const CalibrationData& data);
    void calibrateCenter();
    void calibrateOuterRing();
};

#endif
