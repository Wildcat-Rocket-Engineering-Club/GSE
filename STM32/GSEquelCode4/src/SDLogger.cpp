#include <Arduino.h>

#include "SDLogger.h"

namespace {
constexpr uint8_t PIN_SD_CS = PB12;
constexpr uint8_t PIN_SD_SCK = PB13;
constexpr uint8_t PIN_SD_MISO = PB14;
constexpr uint8_t PIN_SD_MOSI = PB15;
constexpr uint32_t FLUSH_INTERVAL_ROWS = 10;
}

SDLogger::SDLogger()
    : spi(PIN_SD_MOSI, PIN_SD_MISO, PIN_SD_SCK),
      currentFileName{0},
      rowCount(0),
      isReady(false) {}

bool SDLogger::begin(Print &diagnostics) {
  isReady = false;
  spi.begin();

  SdSpiConfig config(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &spi);
  if (!sd.begin(config)) {
    diagnostics.print(F("SD: begin() FAILED, errorCode="));
    diagnostics.print(static_cast<int>(sd.sdErrorCode()));
    diagnostics.print(F(" errorData="));
    diagnostics.println(static_cast<int>(sd.sdErrorData()));
    return false;
  }

  bool foundAvailableName = false;
  for (int index = 1; index <= 999; ++index) {
    snprintf(currentFileName, sizeof(currentFileName), "LOG_%03d.CSV", index);
    if (!sd.exists(currentFileName)) {
      foundAvailableName = true;
      break;
    }
  }

  if (!foundAvailableName) {
    diagnostics.println(F("SD: no unused LOG_001.CSV through LOG_999.CSV filename"));
    return false;
  }

  if (!file.open(currentFileName, O_RDWR | O_CREAT | O_TRUNC)) {
    return reportFileError(diagnostics, F("SD: failed to open log file"));
  }

  diagnostics.print(F("SD: opened "));
  diagnostics.println(currentFileName);

  file.clearWriteError();
  file.println(F("row,bottle_psi,tank_psi,chamber_psi,loadcell_lbs,"
                 "gse_fill,gse_relief,gse_dump,"
                 "rkt_ox,rkt_fuel,rkt_relief,rkt_dump,rkt_ign"));
  if (file.getWriteError() || !file.sync()) {
    return reportFileError(diagnostics, F("SD: failed to write CSV header"));
  }

  rowCount = 0;
  isReady = true;
  diagnostics.println(F("SD: CSV header written"));
  return true;
}

bool SDLogger::logRow(const SDLogRecord &record, Print &diagnostics) {
  if (!isReady) return false;

  const uint32_t nextRow = rowCount + 1;
  file.clearWriteError();
  file.print(nextRow);                  file.print(',');
  file.print(static_cast<int>(record.bottlePsi));  file.print(',');
  file.print(static_cast<int>(record.tankPsi));    file.print(',');
  file.print(static_cast<int>(record.chamberPsi)); file.print(',');
  file.print(record.loadCellLbs, 3);    file.print(',');
  file.print(record.gseFill);           file.print(',');
  file.print(record.gseRelief);         file.print(',');
  file.print(record.gseDump);           file.print(',');
  file.print(record.rocketOx);          file.print(',');
  file.print(record.rocketFuel);        file.print(',');
  file.print(record.rocketRelief);      file.print(',');
  file.print(record.rocketDump);        file.print(',');
  file.println(record.rocketIgnition);

  if (file.getWriteError()) {
    return reportFileError(diagnostics, F("SD: write failed"));
  }

  rowCount = nextRow;
  if (rowCount % FLUSH_INTERVAL_ROWS == 0) {
    if (!file.sync()) {
      return reportFileError(diagnostics, F("SD: sync failed"));
    }
    diagnostics.print(F("SD: rows written="));
    diagnostics.println(rowCount);
  }

  return true;
}

bool SDLogger::ready() const {
  return isReady;
}

const char *SDLogger::fileName() const {
  return currentFileName;
}

bool SDLogger::reportFileError(Print &diagnostics,
                               const __FlashStringHelper *message) {
  diagnostics.print(message);
  diagnostics.print(F(", errorCode="));
  diagnostics.print(static_cast<int>(sd.sdErrorCode()));
  diagnostics.print(F(" errorData="));
  diagnostics.println(static_cast<int>(sd.sdErrorData()));
  isReady = false;
  return false;
}
