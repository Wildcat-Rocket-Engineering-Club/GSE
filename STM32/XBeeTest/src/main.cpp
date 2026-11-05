#include <Arduino.h>

#define ARDUINO_ARCH_STM32 //I don't think this is needed but might as well
/*
* Test the XBee by trying to recieve and send messages
* Uses 2 serial ports: "physical" Serial1 to connect to the XBee and "virtual" Serial to connect to serial monitor via USB
*/
const int notificationInterval = 2000; // ms
const int pingInterval = 1000; // ms

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(230400); //Etan's code uses this for the XBEE
  while (!Serial) {}
  // Send regular messages while waiting
  int lastNotification = 0;
  while (!Serial1) {
    if (millis() - lastNotification >= notificationInterval) {
      Serial.println("Waiting for XBee serial...");
      lastNotification = millis();
    }
  }
  Serial.println("XBee serial available");
}

int lastPingTime = 0;
int pingNumber = 1;
String pingStr;
void loop() {
  //Forward messages from serial monitor to XBee
  if (Serial.available() > 0) {
    incomingUSB = Serial.readString();
    Serial1.write(incomingUSB);
  }
  // Send regular pings and notify on serial monitor when we do so
  if (millis() - lastPingTime >= pingInterval) {
    pingStr = "ping " + pingNumber;
    Serial1.write(pingStr);
    lastPingTime = millis();
    Serial.println(timePrefix(lastPingTime) + "Sent " + pingStr);
    pingNumber++;
  }
  // Notify of messages recieved from XBee
  if (Serial1.available() > 0) {
    incomingXBee = Serial1.readString();
    Serial.println(timePrefix(millis()) + "Recieved \"" + incomingXBee + "\"");
  }
}

String timePrefix(int time_ms) {
  return "[" + time_ms / 60000 + ":" + (time_ms % 60000) / 1000.0 "] ";
}