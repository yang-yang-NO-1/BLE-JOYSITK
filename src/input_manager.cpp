#include "input_manager.h"
#include "config.h"

/**
 * @brief Initialize input pins and ADC resolution, then load or create calibration.
 */
void InputManager::begin() {
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN2_PIN, INPUT_PULLUP);

    analogReadResolution(12);
    store.begin();

    CalibrationData loaded{};
    if (store.load(loaded)) {
        applyCalibrationData(loaded);
        Serial.println("[CAL] Loaded calibration from NVS.");
        printCalibration();
    } else {
        Serial.println("[CAL] No valid calibration found in NVS.");
        recalibrate();
    }
}

/**
 * @brief Apply a calibration structure to the live input manager state.
 */
void InputManager::applyCalibrationData(const CalibrationData& data) {
    centerX = data.centerX;
    centerY = data.centerY;
    minX = data.minX;
    maxX = data.maxX;
    minY = data.minY;
    maxY = data.maxY;

    filteredX = static_cast<float>(centerX);
    filteredY = static_cast<float>(centerY);
    filterInitialized = true;
}

CalibrationData InputManager::getCalibrationData() const {
    CalibrationData data{};
    data.centerX = centerX;
    data.centerY = centerY;
    data.minX = minX;
    data.maxX = maxX;
    data.minY = minY;
    data.maxY = maxY;
    data.valid = true;
    return data;
}

/**
 * @brief Perform averaged ADC sampling on a specified pin.
 *
 * @param pin ADC pin number.
 * @param samples Number of samples to average.
 * @return uint16_t Averaged ADC result.
 */
uint16_t InputManager::readAdcAverage(uint8_t pin, int samples) const {
    uint32_t sum = 0;
    for (int i = 0; i < samples; ++i) {
        sum += analogRead(pin);
        delayMicroseconds(500);
    }
    return static_cast<uint16_t>(sum / samples);
}

/**
 * @brief Clamp an integer to the specified closed interval.
 */
int InputManager::clampInt(int value, int lo, int hi) const {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

/**
 * @brief Map a calibrated raw axis value into 0~4095.
 *
 * The mapping is piecewise linear around the calibrated center:
 * - [min, center] -> [0, 2048]
 * - [center, max] -> [2048, 4095]
 *
 * A deadzone is applied around the center to suppress idle ADC jitter.
 */
int InputManager::mapAxisCalibrated(int raw, int minVal, int centerVal, int maxVal) const {
    raw = clampInt(raw, minVal, maxVal);

    if (abs(raw - centerVal) <= DEADZONE_ADC) {
        return OUTPUT_CENTER;
    }

    if (raw < centerVal) {
        const int inSpan = centerVal - minVal;
        if (inSpan <= 0) return OUTPUT_CENTER;
        const long numerator = static_cast<long>(raw - centerVal) * (OUTPUT_CENTER - OUTPUT_MIN);
        const int out = OUTPUT_CENTER + static_cast<int>(numerator / inSpan);
        return clampInt(out, OUTPUT_MIN, OUTPUT_CENTER);
    }

    const int inSpan = maxVal - centerVal;
    if (inSpan <= 0) return OUTPUT_CENTER;
    const long numerator = static_cast<long>(raw - centerVal) * (OUTPUT_MAX - OUTPUT_CENTER);
    const int out = OUTPUT_CENTER + static_cast<int>(numerator / inSpan);
    return clampInt(out, OUTPUT_CENTER, OUTPUT_MAX);
}

/**
 * @brief Calibrate joystick center while the stick is released.
 */
void InputManager::calibrateCenter() {
    uint32_t sumX = 0;
    uint32_t sumY = 0;

    Serial.println("[CAL] Center calibration start. Keep joystick released.");

    for (int i = 0; i < CENTER_CAL_SAMPLES; ++i) {
        sumX += readAdcAverage(JOY_X_PIN, 4);
        sumY += readAdcAverage(JOY_Y_PIN, 4);
        delay(10);
    }

    centerX = static_cast<int>(sumX / CENTER_CAL_SAMPLES);
    centerY = static_cast<int>(sumY / CENTER_CAL_SAMPLES);

    filteredX = static_cast<float>(centerX);
    filteredY = static_cast<float>(centerY);
    filterInitialized = true;

    Serial.printf("[CAL] Center done: centerX=%d centerY=%d\n", centerX, centerY);
}

/**
 * @brief Calibrate joystick outer ring by sampling min/max while the user moves the stick.
 */
void InputManager::calibrateOuterRing() {
    unsigned long startMs = millis();
    unsigned long lastRemainSec = ULONG_MAX;

    int currentX = readAdcAverage(JOY_X_PIN, 8);
    int currentY = readAdcAverage(JOY_Y_PIN, 8);

    minX = maxX = currentX;
    minY = maxY = currentY;

    Serial.println("[CAL] Outer-ring calibration start.");
    Serial.println("[CAL] Move joystick to all extremes in circles...");

    while (millis() - startMs < OUTER_CAL_MS) {
        currentX = readAdcAverage(JOY_X_PIN, 4);
        currentY = readAdcAverage(JOY_Y_PIN, 4);

        if (currentX < minX) minX = currentX;
        if (currentX > maxX) maxX = currentX;
        if (currentY < minY) minY = currentY;
        if (currentY > maxY) maxY = currentY;

        const unsigned long remainSec = (OUTER_CAL_MS - (millis() - startMs)) / 1000;
        if (remainSec != lastRemainSec) {
            lastRemainSec = remainSec;
            Serial.printf("[CAL] Remaining: %lus minX=%d maxX=%d minY=%d maxY=%d\n",
                          remainSec, minX, maxX, minY, maxY);
        }

        delay(5);
    }

    if (centerX - minX < 100) minX = centerX - 100;
    if (maxX - centerX < 100) maxX = centerX + 100;
    if (centerY - minY < 100) minY = centerY - 100;
    if (maxY - centerY < 100) maxY = centerY + 100;

    minX = clampInt(minX, 0, 4095);
    maxX = clampInt(maxX, 0, 4095);
    minY = clampInt(minY, 0, 4095);
    maxY = clampInt(maxY, 0, 4095);

    Serial.println("[CAL] Outer-ring done.");
    printCalibration();
}

/**
 * @brief Run center calibration and outer-ring calibration consecutively, then save to NVS.
 */
void InputManager::recalibrate() {
    calibrateCenter();
    delay(300);
    calibrateOuterRing();
    delay(300);

    filteredX = static_cast<float>(centerX);
    filteredY = static_cast<float>(centerY);
    filterInitialized = true;

    const bool saved = store.save(getCalibrationData());
    Serial.printf("[CAL] Save to NVS: %s\n", saved ? "OK" : "FAILED");
    Serial.println("[CAL] Full calibration finished.");
}

/**
 * @brief Print the current calibration values.
 */
void InputManager::printCalibration() const {
    Serial.printf("[CAL] Result: minX=%d centerX=%d maxX=%d | minY=%d centerY=%d maxY=%d\n",
                  minX, centerX, maxX, minY, centerY, maxY);
}

/**
 * @brief Determine whether the recalibration button combination has been held long enough.
 */
bool InputManager::shouldRecalibrate(unsigned long nowMs, bool btn1Pressed, bool btn2Pressed) {
    if (btn1Pressed && btn2Pressed) {
        if (recalibPressStart == 0) {
            recalibPressStart = nowMs;
            return false;
        }
        if (nowMs - recalibPressStart >= RECALIB_HOLD_MS) {
            recalibPressStart = 0;
            return true;
        }
        return false;
    }

    recalibPressStart = 0;
    return false;
}

/**
 * @brief Read current raw inputs, update filter state and return normalized output.
 */
InputState InputManager::readState() {
    InputState state{};

    state.rawX = readAdcAverage(JOY_X_PIN, ADC_SAMPLES);
    state.rawY = readAdcAverage(JOY_Y_PIN, ADC_SAMPLES);

    if (!filterInitialized) {
        filteredX = state.rawX;
        filteredY = state.rawY;
        filterInitialized = true;
    } else {
        filteredX = filteredX * (1.0f - FILTER_ALPHA) + state.rawX * FILTER_ALPHA;
        filteredY = filteredY * (1.0f - FILTER_ALPHA) + state.rawY * FILTER_ALPHA;
    }

    const int smoothX = static_cast<int>(filteredX + 0.5f);
    const int smoothY = static_cast<int>(filteredY + 0.5f);

    state.outX = static_cast<uint16_t>(mapAxisCalibrated(smoothX, minX, centerX, maxX));
    state.outY = static_cast<uint16_t>(mapAxisCalibrated(smoothY, minY, centerY, maxY));

    state.joySwitchPressed = (digitalRead(JOY_SW_PIN) == LOW);
    state.btn1Pressed = (digitalRead(BTN1_PIN) == LOW);
    state.btn2Pressed = (digitalRead(BTN2_PIN) == LOW);

    return state;
}
