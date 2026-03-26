#ifndef CALIBRATION_STORE_H
#define CALIBRATION_STORE_H

#include <Arduino.h>

/**
 * @brief Persistent joystick calibration parameters.
 */
struct CalibrationData {
    int centerX;
    int centerY;
    int minX;
    int maxX;
    int minY;
    int maxY;
    bool valid;
};

/**
 * @brief Simple NVS-backed storage for joystick calibration data.
 */
class CalibrationStore {
public:
    /**
     * @brief Initialize the Preferences namespace.
     */
    void begin();

    /**
     * @brief Load calibration data from NVS.
     * @param out Filled with stored values when available.
     * @return true if valid calibration data was found.
     */
    bool load(CalibrationData& out);

    /**
     * @brief Save calibration data to NVS.
     * @param data Calibration values to persist.
     * @return true on success.
     */
    bool save(const CalibrationData& data);

    /**
     * @brief Remove stored calibration data from NVS.
     * @return true on success.
     */
    bool clear();
};

#endif
