#include "calibration_store.h"
#include <Preferences.h>

namespace {
constexpr const char* NS_NAME = "joycal";
constexpr uint32_t MAGIC = 0x4A43414CUL; // "JCAL"
Preferences prefs;
}

void CalibrationStore::begin() {
    prefs.begin(NS_NAME, false);
}

bool CalibrationStore::load(CalibrationData& out) {
    const uint32_t magic = prefs.getULong("magic", 0);
    if (magic != MAGIC) {
        out.valid = false;
        return false;
    }

    out.centerX = prefs.getInt("centerX", 2048);
    out.centerY = prefs.getInt("centerY", 2048);
    out.minX = prefs.getInt("minX", 0);
    out.maxX = prefs.getInt("maxX", 4095);
    out.minY = prefs.getInt("minY", 0);
    out.maxY = prefs.getInt("maxY", 4095);
    out.valid = true;
    return true;
}

bool CalibrationStore::save(const CalibrationData& data) {
    bool ok = true;
    ok &= prefs.putInt("centerX", data.centerX) > 0;
    ok &= prefs.putInt("centerY", data.centerY) > 0;
    ok &= prefs.putInt("minX", data.minX) > 0;
    ok &= prefs.putInt("maxX", data.maxX) > 0;
    ok &= prefs.putInt("minY", data.minY) > 0;
    ok &= prefs.putInt("maxY", data.maxY) > 0;
    ok &= prefs.putULong("magic", MAGIC) > 0;
    return ok;
}

bool CalibrationStore::clear() {
    return prefs.clear();
}
