#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include <Print.h>

#include "ADS1256PressureSensor.h"
#include "ValveController.h"

enum class SerialCommandType : uint8_t {
  None,
  Launch,
  OpenAll,
  CloseAll,
  SetValve
};

struct SerialCommand {
  SerialCommandType type;
  ValveId valve;
  int state;
};

struct SerialTelemetry {
  PressureReadings pressure;
  float loadCellLbs;
  ValveStates valves;
};

class SerialProtocol {
public:
  bool receive(char character, SerialCommand &command);
  void writeTelemetry(Print &output, const SerialTelemetry &telemetry) const;

private:
  String lineBuffer;

  bool parseCommand(const String &line, SerialCommand &command) const;
};

#endif
