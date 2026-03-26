#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =========================
// Hardware pin definition
// =========================
static constexpr uint8_t JOY_X_PIN  = 34;
static constexpr uint8_t JOY_Y_PIN  = 35;
static constexpr uint8_t JOY_SW_PIN = 25;
static constexpr uint8_t BTN1_PIN   = 26;
static constexpr uint8_t BTN2_PIN   = 27;

// =========================
// BLE UUID definition
// =========================
static constexpr const char* DEVICE_NAME         = "ESP32_Gamepad";
static constexpr const char* SERVICE_UUID        = "12345678-1234-1234-1234-1234567890ab";
static constexpr const char* CHARACTERISTIC_UUID = "abcd1234-5678-1234-5678-1234567890ab";

// =========================
// ADC and filter parameters
// =========================
static constexpr int ADC_SAMPLES       = 8;
static constexpr int DEADZONE_ADC      = 20;
static constexpr float FILTER_ALPHA    = 0.30f;
static constexpr int OUTPUT_MIN        = 0;
static constexpr int OUTPUT_CENTER     = 2048;
static constexpr int OUTPUT_MAX        = 4095;

// =========================
// Calibration timing
// =========================
static constexpr int CENTER_CAL_SAMPLES      = 80;
static constexpr unsigned long OUTER_CAL_MS  = 6000;
static constexpr unsigned long RECALIB_HOLD_MS = 2000;

#endif
