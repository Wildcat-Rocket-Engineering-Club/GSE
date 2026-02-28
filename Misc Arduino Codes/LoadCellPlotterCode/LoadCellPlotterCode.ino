//
//    FILE: HX_plotter.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: HX711 demo
//     URL: https://github.com/RobTillaart/HX711


#include "HX711.h"

HX711 scale;

const int sampleSize = 25;
uint8_t dataPin = 2;
uint8_t clockPin = 3;

uint32_t start, stop;
volatile float f;
double urmom[sampleSize];
int i=0;


void setup()
{
  // Initializes.
  for (int i=0; i<sampleSize; i++)
  {
    urmom[i] = 0;
  }
  Serial.begin(9600);
  // Serial.println(__FILE__);
  // Serial.print("LIBRARY VERSION: ");
  // Serial.println(HX711_LIB_VERSION);
  // Serial.println();

  scale.begin(dataPin, clockPin);

  // Sets offsets and scales
  scale.set_offset(0);
  scale.set_scale(0);
  scale.tare();  
  // reset the scale to zero = 0
}


void loop()
{
  // continuous scale 4x per second
  f = scale.get_units(0);
  urmom[i] = (double)f;
  int count = 1;
  double avg= 0;
  if(i==sampleSize-1)
  {
    for(int a = 0; a<sampleSize; a++){
        avg +=urmom[a];
        count++;
    }
    avg /=count;
    i = -1;
      Serial.println((String)sampleSize+"-unit avg: "+(String)avg);
       for (int i=0; i<sampleSize; i++)
  {
    urmom[i] = 0;
  }

  }
  i++;
}

// -- END OF FILE --
// We measured 43370 units for 10.785 kg
