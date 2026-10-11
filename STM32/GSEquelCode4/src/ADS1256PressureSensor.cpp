#include <Arduino.h>

#include "ADS1256PressureSensor.h"

namespace {
constexpr int8_t PIN_ADS1256_DRDY = PA8;
constexpr int8_t PIN_ADS1256_CS = PA4;
constexpr unsigned long READY_TIMEOUT_MS = 1500;
constexpr float MAX_VOLTAGE = 5.0f;
constexpr float MAX_PRESSURE_PSI = 5000.0f;
constexpr float BOTTLE_PRESSURE_OFFSET_PSI = -39.0f;
constexpr float TANK_PRESSURE_OFFSET_PSI = -36.0f;
constexpr float CHAMBER_PRESSURE_OFFSET_PSI = -53.0f;

float voltageToPressurePsi(float voltage, float offsetPsi) {
  if (voltage < 0.0f) voltage = 0.0f;
  if (voltage > MAX_VOLTAGE) voltage = MAX_VOLTAGE;
  return (voltage / MAX_VOLTAGE) * MAX_PRESSURE_PSI + offsetPsi;
}
}

ADS1256PressureSensor::ADS1256PressureSensor()
    : adc(PIN_ADS1256_DRDY, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED,
          PIN_ADS1256_CS, 2.500, &SPI),
      rawBottle(0),
      rawTank(0),
      rawChamber(0),
      ready(false) {}

bool ADS1256PressureSensor::begin() {
  // Keep an unconnected DRDY pin high so a missing module cannot look ready.
  pinMode(PIN_ADS1256_DRDY, INPUT_PULLUP);
  const unsigned long startTime = millis();
  while (digitalRead(PIN_ADS1256_DRDY) != LOW &&
         millis() - startTime < READY_TIMEOUT_MS) {
    delay(1);
  }

  if (digitalRead(PIN_ADS1256_DRDY) != LOW) {
    ready = false;
    return false;
  }

  adc.InitializeADC();
  adc.setPGA(PGA_1);
  adc.setBuffer(0);
  adc.setDRATE(DRATE_2000SPS);
  delay(100);
  adc.sendDirectCommand(SELFCAL);
  delay(200);

  ready = true;
  return true;
}

bool ADS1256PressureSensor::update(PressureReadings &readings) {
  if (!ready) return false;

  const uint8_t channels[3] = {SING_0, SING_1, SING_2};
  const ADS1256::ADS1256_Result result = adc.cycleSingle3Tracked(channels, 3);
  if (result.channel == 0xFF) return false;

  if      (result.channel == SING_2) rawBottle = result.value;
  else if (result.channel == SING_0) rawTank = result.value;
  else if (result.channel == SING_1) {
    rawChamber = result.value;

    readings.bottlePsi =
        voltageToPressurePsi(adc.convertToVoltage(rawBottle), BOTTLE_PRESSURE_OFFSET_PSI);
    readings.tankPsi =
        voltageToPressurePsi(adc.convertToVoltage(rawTank), TANK_PRESSURE_OFFSET_PSI);
    readings.chamberPsi =
        voltageToPressurePsi(adc.convertToVoltage(rawChamber), CHAMBER_PRESSURE_OFFSET_PSI);
    return true;
  }

  return false;
}
