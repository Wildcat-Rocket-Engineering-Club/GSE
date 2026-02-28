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

bool enableLogging = false;
HX711 scale;

uint8_t dataPin = 2;
uint8_t clockPin = 3;

uint32_t start, stop;
volatile float f;
String dataString;
int timer;

//variables used for button toggle debounce
const int button = 4;
unsigned long debounceDuration = 50; // millis
unsigned long lastTimeButtonStateChanged = 0;
byte lastButtonState = LOW;
byte buttonState = LOW;

unsigned long logDuration = 0; // millis

void setup() {

  pinMode(button, INPUT);
  Serial.begin(115200);
  
  // Check SD card initialization
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  
  Serial.println("SD card initialized.");
  SD.begin(chipSelect);

  scale.begin(dataPin, clockPin);
  scale.set_scale(-4209.2);     // TODO you need to calibrate this yourself.
  // reset the scale to zero = 0
  scale.tare();
}


void loop() {
    if (enableLogging) {
      f = scale.get_units(5);
      dataString = String((millis()) - logDuration) + ", " + String(f); // Build data string
      Serial.println(dataString);
    
      //Open the file and write the force data. also log it in console!
      File dataFile = SD.open("datalog1.txt", FILE_WRITE);
      
      if (dataFile) {
        dataFile.println( dataString );
        // print to the serial port too:
        Serial.println(dataString);
        dataFile.close();
      }
      else {
        Serial.println("error opening datalog.txt");
      }
    }

// button toggle Debounce
    if (millis() - lastTimeButtonStateChanged > debounceDuration) {
    byte buttonState = digitalRead(button);
   if (buttonState != lastButtonState) {
      lastTimeButtonStateChanged = millis();
      lastButtonState = buttonState;
      
      buttonState = (buttonState == HIGH) ? LOW: HIGH;

      if(buttonState == HIGH){
      // Toggles logging when the button is pressed.
      if(enableLogging){
              Serial.println("Ending Logging.");
      }
      else
      {
                Serial.println("Beginning Logging!");
                logDuration = millis();
      }
      enableLogging = !enableLogging;
      }

      /*
      if(enableLogging)
      {
        Serial.println("Opening File");
      }
      else
      {
        Serial.println("Closing File");
      }
      */
      /*
      if (buttonState == LOW)
        Serial.println("Closing File");
      else
        Serial.println("Opening File");

        */

      
    }
  }
  
}
