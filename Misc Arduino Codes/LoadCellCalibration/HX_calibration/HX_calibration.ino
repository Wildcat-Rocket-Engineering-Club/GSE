// FILE: HX_calibration_decimal.ino
// AUTHOR: Rob Tillaart / Adapted for Float Input
// PURPOSE: HX711 calibration with decimal support

#include "HX711.h"

HX711 myScale;

uint8_t dataPin = 2;
uint8_t clockPin = 3;

void setup() {
  Serial.begin(9600);
  Serial.println(__FILE__);
  Serial.print("LIBRARY VERSION: ");
  Serial.println(HX711_LIB_VERSION);
  Serial.println();

  myScale.begin(dataPin, clockPin);
}

void loop() {
  calibrate();
}

void calibrate() {
  Serial.println("\n\nCALIBRATION\n===========");
  Serial.println("1. Remove all weight from the loadcell.");
  
  // Flush any leftover characters in the buffer
  while (Serial.available() > 0) Serial.read();
  
  Serial.println("Press Enter (or send any character) to set zero offset...");
  while (Serial.available() == 0); // Wait for user input
  while (Serial.available() > 0) Serial.read(); // Clear that input

  Serial.println("Determining zero weight offset...");
  myScale.tare(20); 
  uint32_t offset = myScale.get_offset();
  Serial.print("OFFSET: ");
  Serial.println(offset);
  Serial.println();

  Serial.println("2. Place a known weight on the loadcell.");
  Serial.println("Enter the weight value (e.g. 500.50) and press Enter:");

  // Wait for the user to start typing the weight
  while (Serial.available() == 0);

  // Read the decimal value from Serial
  float weight = Serial.parseFloat();

  if (weight == 0) {
    Serial.println("Error: Received 0.00 or invalid input. Restarting...");
    return;
  }

  Serial.print("WEIGHT ENTERED: ");
  Serial.println(weight, 3);

  Serial.println("Calculating scale factor...");
  myScale.calibrate_scale(weight, 20);
  float scale = myScale.get_scale();

  Serial.print("SCALE: ");
  Serial.println(scale, 6);
  
  Serial.println("\n--- COPY THESE TO YOUR PROJECT SETUP ---");
  Serial.print("scale.set_offset(");
  Serial.print(offset);
  Serial.println(");");
  Serial.print("scale.set_scale(");
  Serial.print(scale, 6);
  Serial.println(");");
  Serial.println("-----------------------------------------\n");

  Serial.println("Calibration cycle finished. Restarting in 5 seconds...");
  delay(5000);
}
