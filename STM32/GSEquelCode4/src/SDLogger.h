#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SdFat.h>

struct SDLogRecord {
  float bottlePsi;
  float tankPsi;
  float chamberPsi;
  float loadCellLbs;
  int gseFill;
  int gseRelief;
  int gseDump;
  int rocketOx;
  int rocketFuel;
  int rocketRelief;
  int rocketDump;
  int rocketIgnition;
};

class SDLogger {
public:
  SDLogger();

  bool begin(Print &diagnostics);
  bool logRow(const SDLogRecord &record, Print &diagnostics);
  bool ready() const;
  const char *fileName() const;

private:
  SPIClass spi;
  SdFat sd;
  SdFile file;
  char currentFileName[16];
  uint32_t rowCount;
  bool isReady;

  bool reportFileError(Print &diagnostics, const __FlashStringHelper *message);
};

#endif
