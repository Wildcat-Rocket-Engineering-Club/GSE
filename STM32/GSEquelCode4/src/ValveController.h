#ifndef VALVE_CONTROLLER_H
#define VALVE_CONTROLLER_H

#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>
#include <Print.h>

enum class ValveId : uint8_t {
  GseFill,
  GseRelief,
  GseDump,
  RocketOx,
  RocketFuel,
  RocketRelief,
  RocketDump,
  RocketIgnition
};

struct ValveStates {
  int gseFill;
  int gseRelief;
  int gseDump;
  int rocketOx;
  int rocketFuel;
  int rocketRelief;
  int rocketDump;
  int rocketIgnition;
};

class ValveController {
public:
  ValveController();

  void begin(Print &diagnostics);
  void setValve(ValveId valve, int newState);
  void openAll(Print &diagnostics);
  void closeAll(Print &diagnostics);
  void startLaunch(Print &diagnostics);
  void updateLaunchSequence(Print &diagnostics);
  const ValveStates &states() const;

private:
  enum class LaunchState : uint8_t {
    Idle,
    Ignition,
    OxidizerPending,
    FuelPending,
    Complete
  };

  TwoWire wire;
  Adafruit_PWMServoDriver pwm;
  ValveStates valveStates;
  LaunchState launchState;
  unsigned long launchStartTime;

  void servoWriteUs(uint8_t channel, uint16_t pulseWidthUs);
};

#endif
