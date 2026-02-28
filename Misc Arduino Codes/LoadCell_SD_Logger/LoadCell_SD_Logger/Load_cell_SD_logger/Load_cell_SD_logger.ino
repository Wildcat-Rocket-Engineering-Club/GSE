//
//    FILE: HX_plotter.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: HX711 demo
//     URL: https://github.com/RobTillaart/HX711


#include "HX711.h"
#include <SPI.h>
#include <SD.h>

//SD card stuff
const int chipSelect = 10;


HX711 scale;

uint8_t dataPin = 2;
uint8_t clockPin = 3;

uint32_t start, stop;
volatile float f;
String dataString;
int timer;

void setup() {
  //button to stop datalogger
  const int button = 4;
  int buttonValue = 0;

  pinMode(button, INPUT_PULLUP);
  Serial.begin(115200);
  
  // Check SD card initialization
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  
  Serial.println("SD card initialized.");
  SD.begin(chipSelect);

  scale.begin(dataPin, clockPin);
  scale.set_scale(-4040);       // TODO you need to calibrate this yourself.
  // reset the scale to zero = 0
  scale.tare();
  
  while (buttonValue == LOW) {
    buttonValue = digitalRead(button);
     f = scale.get_units(5);
    dataString = String(millis()) + ", " + String(f); // Build data string
    Serial.println(dataString);
  
    //Open the file and write the force data. also log it in console!
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
    
    if (dataFile) {
      dataFile.println( dataString );
      // print to the serial port too:
      Serial.println(dataString);
      dataFile.close();
    }
    else {
      Serial.println("error opening datalog.txt");
    }
    delay(100); // Small delay for stability
  }
  Serial.println("Closing File");
}


// -- END OF FILE --

void loop() {
  // nothing happens after setup
}
