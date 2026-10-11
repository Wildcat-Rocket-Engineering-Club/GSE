#include <Arduino.h>

#include "HX711LoadCell.h"

namespace {
constexpr uint8_t PIN_HX711_DOUT = PC6;
constexpr uint8_t PIN_HX711_SCK = PC7;
constexpr int32_t HX711_OFFSET = 10304;
constexpr float HX711_SCALE = 1891.308715f;
constexpr float ROCKET_DRY_WEIGHT_LBS = 85.0f;
constexpr unsigned long READY_TIMEOUT_MS = 1000;
constexpr uint8_t AVERAGE_SAMPLES = 4;
}

HX711LoadCell::HX711LoadCell()
    : loadLbs(0.0f),
      ready(false) {}

bool HX711LoadCell::begin(Print &diagnostics) {
  scale.begin(PIN_HX711_DOUT, PIN_HX711_SCK, true);

  const unsigned long startTime = millis();
  while (!scale.is_ready() && millis() - startTime < READY_TIMEOUT_MS) {
  }

  if (!scale.is_ready()) {
    ready = false;
    diagnostics.println(F("HX711: not ready — check wiring"));
    return false;
  }

  scale.set_offset(HX711_OFFSET);
  scale.set_scale(HX711_SCALE);
  ready = true;

  diagnostics.print(F("HX711: ready, offset="));
  diagnostics.print(HX711_OFFSET);
  diagnostics.print(F(", scale="));
  diagnostics.println(HX711_SCALE);
  return true;
}

void HX711LoadCell::update() {
  if (!ready || !scale.is_ready()) return;

  loadLbs = -scale.get_units(AVERAGE_SAMPLES) - ROCKET_DRY_WEIGHT_LBS;
}

float HX711LoadCell::valueLbs() const {
  return loadLbs;
}
