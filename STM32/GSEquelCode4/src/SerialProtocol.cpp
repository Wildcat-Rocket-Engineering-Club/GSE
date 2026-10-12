#include "SerialProtocol.h"

bool SerialProtocol::receive(char character, SerialCommand &command) {
  if (character == '\n') {
    const bool recognized = parseCommand(lineBuffer, command);
    lineBuffer = "";
    return recognized;
  }

  if (character != '\r') {
    lineBuffer += character;
  }
  return false;
}

bool SerialProtocol::parseCommand(const String &line, SerialCommand &command) const {
  command = {SerialCommandType::None, ValveId::GseFill, 0};

  if (line.indexOf("\"launch\"") != -1 || line.indexOf("launch") != -1) {
    command.type = SerialCommandType::Launch;
    return true;
  }

  if (line.indexOf("\"oa\"") != -1 || line.indexOf("oa") != -1) {
    command.type = SerialCommandType::OpenAll;
    return true;
  }

  if (line.indexOf("\"ca\"") != -1 || line.indexOf("ca") != -1) {
    command.type = SerialCommandType::CloseAll;
    return true;
  }

  if (line.indexOf("set_valve") == -1) return false;

  const int stateIndex = line.indexOf("\"state\":");
  if (stateIndex != -1) {
    int valueIndex = stateIndex + 8;
    while (valueIndex < static_cast<int>(line.length()) && line[valueIndex] == ' ') {
      ++valueIndex;
    }
    if (valueIndex < static_cast<int>(line.length())) {
      command.state = line[valueIndex] == '1' ? 1 : 0;
    }
  }

  if      (line.indexOf("\"gf\"") != -1 || line.indexOf("gse_fill")      != -1) command.valve = ValveId::GseFill;
  else if (line.indexOf("\"gr\"") != -1 || line.indexOf("gse_relief")    != -1) command.valve = ValveId::GseRelief;
  else if (line.indexOf("\"gd\"") != -1 || line.indexOf("gse_dump")      != -1) command.valve = ValveId::GseDump;
  else if (line.indexOf("\"ro\"") != -1 || line.indexOf("rocket_ox")     != -1) command.valve = ValveId::RocketOx;
  else if (line.indexOf("\"rf\"") != -1 || line.indexOf("rocket_fuel")   != -1) command.valve = ValveId::RocketFuel;
  else if (line.indexOf("\"rr\"") != -1 || line.indexOf("rocket_relief") != -1) command.valve = ValveId::RocketRelief;
  else if (line.indexOf("\"rd\"") != -1 || line.indexOf("rocket_dump")   != -1) command.valve = ValveId::RocketDump;
  else if (line.indexOf("\"ig\"") != -1 || line.indexOf("ignite")        != -1) command.valve = ValveId::RocketIgnition;
  else return false;

  command.type = SerialCommandType::SetValve;
  return true;
}

void SerialProtocol::writeTelemetry(
    Print &output, const SerialTelemetry &telemetry) const {
  output.print(F("{\"p\":{\"b\":"));
  output.print(static_cast<int>(telemetry.pressure.bottlePsi));
  output.print(F(",\"t\":"));
  output.print(static_cast<int>(telemetry.pressure.tankPsi));
  output.print(F(",\"c\":"));
  output.print(static_cast<int>(telemetry.pressure.chamberPsi));
  output.print(F("},\"g\":{\"l\":"));
  output.print(telemetry.loadCellLbs, 2);
  output.print(F(",\"f\":"));
  output.print(telemetry.valves.gseFill);
  output.print(F(",\"r\":"));
  output.print(telemetry.valves.gseRelief);
  output.print(F(",\"d\":"));
  output.print(telemetry.valves.gseDump);
  output.print(F("},\"r\":{\"o\":"));
  output.print(telemetry.valves.rocketOx);
  output.print(F(",\"f\":"));
  output.print(telemetry.valves.rocketFuel);
  output.print(F(",\"r\":"));
  output.print(telemetry.valves.rocketRelief);
  output.print(F(",\"d\":"));
  output.print(telemetry.valves.rocketDump);
  output.print(F(",\"i\":"));
  output.print(telemetry.valves.rocketIgnition);
  output.println(F("}}"));
}
