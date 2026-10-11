#ifndef ADS1256_PRESSURE_SENSOR_H
#define ADS1256_PRESSURE_SENSOR_H

#include <ADS1256.h>

struct PressureReadings {
  float bottlePsi;
  float tankPsi;
  float chamberPsi;
};

class ADS1256PressureSensor {
public:
  ADS1256PressureSensor();

  bool begin();
  bool update(PressureReadings &readings);

private:
  ADS1256 adc;
  int32_t rawBottle;
  int32_t rawTank;
  int32_t rawChamber;
  bool ready;
};

#endif
