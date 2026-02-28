//
//    OG FILE: HX_plotter.ino
//  AUTHOR: Rob Tillaart
// PURPOSE: HX711 demo
//     URL: https://github.com/RobTillaart/HX711
// This edition is designed to be triggered by a  


#include "HX711.h"
#include <SPI.h>
#include <SD.h>

const bool debug = false;
const double convertFactor =-0.00232083132251;

//SD card stuff
const int chipSelect = 10;


//PWM stuff
const int PWM = 4; //Pin the receiver is on.
bool flip;
bool lastState;
long lastChanged = 0;


HX711 scale;

uint8_t dataPin = 2;
uint8_t clockPin = 3;

uint32_t start, stop;
volatile float f;
String dataString;
int timer;

//variables used for button toggle debounce

unsigned long logDuration = 0; // millis

void setup() {

  pinMode(PWM, INPUT);
  Serial.begin(9600);
  
  // Check SD card initialization
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  
  Serial.println("SD card initialized.");
  SD.begin(chipSelect);

  scale.begin(dataPin, clockPin);
  scale.set_scale(0);     // We found this using known masses on a scale. Outputs kg
  // reset the scale to zero = 0
  scale.tare();
  lastState = (pulseIn(PWM,HIGH)<2000);
}


void loop() {

  
  flip = (pulseIn(PWM,HIGH)<2000);

debugLog();

  if(flip!=lastState)
  {
    lastChanged = millis();
    lastState = flip;
    logDuration = 0;

    if(debug)
    {
      debugLog();
    }
  }
  debugLog();
    logDuration =  millis() - lastChanged;
    /*if(logDuration>68000)
    {
      logDuration-=68000;
    }
    else if(logDuration>65000)
    {
      logDuration-=65000;
    }
    */
    if(!flip)
  {
    logDuration=0;
  }
    debugLog();
    if (flip) {
      debugLog();
      f = scale.get_units(0);
      f = f * (convertFactor)
      dataString = String( logDuration) + ", " + String(f); // Build data string
      Serial.println(dataString);
    
      //Open the file and write the force data. also log it in console!
      File dataFile = SD.open("datalog1.txt", FILE_WRITE);
      
      if (dataFile) {
        debugLog();
        dataFile.println( dataString );
        // print to the serial port too:
        Serial.println(dataString);
        dataFile.close();
      }
      else {
        Serial.println("error opening datalog.txt");
      }
    }
    debugLog();
    

    //Log when the PWM is high and changed. Stop logging when it's low again.

// button toggle Debounce
    /*if (millis() - lastTimeButtonStateChanged > debounceDuration) {
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

      // This part is normally commented.
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

        

      
    }
  }*/
  
}

void debugLog()
{
  if(debug)
  {
    Serial.println("Switch state: " + String(flip)+", last state: "+ String(lastState));
    Serial.println("Time since last change: " + String(lastChanged));
    Serial.println("Current time: "+ String(millis()));
    Serial.println("logDuration: "+String(logDuration));
    Serial.println("Force: "+String(f));
    delay(200);
  }
}
