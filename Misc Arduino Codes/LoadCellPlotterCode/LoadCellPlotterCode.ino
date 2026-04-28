//    FILE: HX_plotter.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: HX711 demo
//     URL: https://github.com/RobTillaart/HX711


#include "HX711.h"

HX711 scale;

const int sampleSize = 5;
uint8_t dataPin = 2;
uint8_t clockPin = 3;

uint32_t start, stop;
volatile float f;
double values[sampleSize];
int i = 0;

bool verboseLog = false;

void setup() {
  // Initializes.
  for (int i = 0; i < sampleSize; i++) {
    values[i] = 0;
  }
  Serial.begin(9600);

  if (verboseLog) {
    Serial.println(__FILE__);
    Serial.print("LIBRARY VERSION: ");
    Serial.println(HX711_LIB_VERSION);
    Serial.println();
  }

  scale.begin(dataPin, clockPin);

  // Sets offsets and scales
  scale.set_offset(11904);
  scale.set_scale(1891.308715);
  //scale.tare();  
  // reset the scale to zero = 0
}


void loop() {
  // continuous scale 4x per second
  f = scale.get_units(0);
  values[i % sampleSize] = (double)f;
  double total = 0;
  if(i % sampleSize == 0) {
    for(int a = 0; a < sampleSize; a++) {
        total += values[a];
    }
    double average = total / sampleSize;
    if (verboseLog) {
      Serial.print((String)sampleSize + "-unit avg: ");
    }
    Serial.println(average);
    for (int i=0; i<sampleSize; i++) {
      values[i] = 0;
    }
  }
  i++;
}

// -- END OF FILE --
// We measured 43370 units for 10.785 kg
