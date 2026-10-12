#include <Arduino.h>

#include "ValveController.h"

namespace {
constexpr uint8_t PCA9685_ADDRESS = 0x40;
constexpr uint8_t PCA9685_FREQUENCY_HZ = 50;
constexpr uint8_t PIN_I2C_SDA = PB9;
constexpr uint8_t PIN_I2C_SCL = PB8;

struct ServoConfig {
  ValveId valve;
  uint8_t channel;
  uint16_t closedUs;
  uint16_t openUs;
};

const ServoConfig SERVOS[] = {
    {ValveId::GseFill,       0, 2510, 1775},
    {ValveId::GseRelief,     1, 2050,  970},
    {ValveId::RocketIgnition,2,  875, 2000},
    {ValveId::RocketOx,      3, 3025,  750},
    {ValveId::RocketFuel,    4, 2200,  960},
    {ValveId::RocketRelief,  5, 2100,  944},
    {ValveId::RocketDump,    6, 2000,  875},
    {ValveId::GseDump,       7, 1930,  970},
};
constexpr size_t SERVO_COUNT = sizeof(SERVOS) / sizeof(SERVOS[0]);

constexpr unsigned long IGNITION_TIME_MS = 0;
constexpr unsigned long OXIDIZER_TIME_MS = 500;
constexpr unsigned long FUEL_TIME_MS = 750;
}

ValveController::ValveController()
    : wire(PIN_I2C_SDA, PIN_I2C_SCL),
      pwm(PCA9685_ADDRESS, wire),
      valveStates{0, 0, 0, 0, 0, 0, 0, 0},
      launchState(LaunchState::Idle),
      launchStartTime(0) {}

void ValveController::begin(Print &diagnostics) {
  wire.begin();
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(PCA9685_FREQUENCY_HZ);
  delay(10);

  for (size_t index = 0; index < SERVO_COUNT; ++index) {
    servoWriteUs(SERVOS[index].channel, SERVOS[index].closedUs);
  }

  diagnostics.println(F("PCA9685: all valves closed"));
}

void ValveController::setValve(ValveId valve, int newState) {
  int *state = nullptr;
  switch (valve) {
    case ValveId::GseFill:        state = &valveStates.gseFill; break;
    case ValveId::GseRelief:      state = &valveStates.gseRelief; break;
    case ValveId::GseDump:        state = &valveStates.gseDump; break;
    case ValveId::RocketOx:       state = &valveStates.rocketOx; break;
    case ValveId::RocketFuel:     state = &valveStates.rocketFuel; break;
    case ValveId::RocketRelief:   state = &valveStates.rocketRelief; break;
    case ValveId::RocketDump:     state = &valveStates.rocketDump; break;
    case ValveId::RocketIgnition: state = &valveStates.rocketIgnition; break;
  }

  if (state == nullptr) return;
  *state = newState;

  for (size_t index = 0; index < SERVO_COUNT; ++index) {
    if (SERVOS[index].valve == valve) {
      servoWriteUs(SERVOS[index].channel,
                   newState == 1 ? SERVOS[index].openUs : SERVOS[index].closedUs);
      delay(15);
      return;
    }
  }
}

void ValveController::openAll(Print &diagnostics) {
  diagnostics.println(F("Opening all valves..."));
  setValve(ValveId::GseFill, 1);
  delay(5);
  setValve(ValveId::GseRelief, 1);
  delay(5);
  setValve(ValveId::GseDump, 1);
  delay(5);
  setValve(ValveId::RocketOx, 1);
  delay(5);
  setValve(ValveId::RocketFuel, 1);
  delay(5);
  setValve(ValveId::RocketRelief, 1);
  delay(5);
  setValve(ValveId::RocketDump, 1);
  delay(5);
  setValve(ValveId::RocketIgnition, 1);
  diagnostics.println(F("All valves opened"));
}

void ValveController::closeAll(Print &diagnostics) {
  diagnostics.println(F("Closing all valves..."));
  setValve(ValveId::GseFill, 0);
  delay(5);
  setValve(ValveId::GseRelief, 0);
  delay(5);
  setValve(ValveId::GseDump, 0);
  delay(5);
  setValve(ValveId::RocketOx, 0);
  delay(5);
  setValve(ValveId::RocketFuel, 0);
  delay(5);
  setValve(ValveId::RocketRelief, 0);
  delay(5);
  setValve(ValveId::RocketDump, 0);
  delay(5);
  setValve(ValveId::RocketIgnition, 0);
  diagnostics.println(F("All valves closed"));
}

void ValveController::startLaunch(Print &diagnostics) {
  if (launchState != LaunchState::Idle && launchState != LaunchState::Complete) {
    diagnostics.println(F("LAUNCH SEQUENCE ALREADY ACTIVE; duplicate ignored"));
    return;
  }

  launchState = LaunchState::Ignition;
  launchStartTime = millis();
  diagnostics.println(F("LAUNCH SEQUENCE INITIATED"));
}

void ValveController::updateLaunchSequence(Print &diagnostics) {
  if (launchState == LaunchState::Idle) return;

  const unsigned long elapsed = millis() - launchStartTime;
  if (launchState == LaunchState::Ignition && elapsed >= IGNITION_TIME_MS) {
    setValve(ValveId::RocketIgnition, 1);
    diagnostics.println(F("[LAUNCH] T+0ms: IGNITION"));
    launchState = LaunchState::OxidizerPending;
  }

  if (launchState == LaunchState::OxidizerPending && elapsed >= OXIDIZER_TIME_MS) {
    setValve(ValveId::RocketOx, 1);
    diagnostics.println(F("[LAUNCH] T+500ms: MAIN OX OPEN"));
    launchState = LaunchState::FuelPending;
  }

  if (launchState == LaunchState::FuelPending && elapsed >= FUEL_TIME_MS) {
    setValve(ValveId::RocketFuel, 1);
    diagnostics.println(F("[LAUNCH] T+750ms: MAIN FUEL OPEN"));
    launchState = LaunchState::Complete;
  }
}

const ValveStates &ValveController::states() const {
  return valveStates;
}

void ValveController::servoWriteUs(uint8_t channel, uint16_t pulseWidthUs) {
  const uint16_t tick = static_cast<uint16_t>((pulseWidthUs / 20000.0f) * 4096.0f);
  pwm.setPWM(channel, 0, tick);
}
