#include "HX711.h"
#include <SPI.h>
#include <SD.h>

#define debug false;
#define dataPin 2;
#define clockPin 3;
#define PWM 4;
#define chipSelect 10;
const double convertFactor =-0.00232083132251;
//int timer;
//uint32_t start, stop;

bool flip, lastState;
long lastChanged = 0;
volatile double val;
HX711 scale;
File dataFile;
String dataString;

// variables used for button toggle debounce
unsigned long logDuration = 0; // millis
double getScaleValue(int samplesize);

void setup() {
    pinMode(PWM, INPUT);
    Serial.begin(9600);
    // Check SD card initialization
    String filename_base = String("datalog_");
    String file_extension = String(".csv");
    int file_num = 0;
    // initialize SPI
    if (!SD.begin(chipSelect)) {
        Serial.println("SD card initialization failed!");
        while(1);
    }
    Serial.println("SD card initialized.");
    // find next available file name
    while(SD.exists(filename_base + file_num + filename_extension))){
        file_num++;
    }
    dataFile = SD.open(filename_base + file_num + filename_extension, FILE_WRITE);
    // initialize csv header
    dataFile.println("Time (s),Force (N)")
    // setup scale
    scale.begin(dataPin, clockPin);
    scale.set_offset(0);
    scale.set_scale(0);
    scale.tare();

    lastState = (pulseIn(PWM,HIGH)<2000);
}

void loop() {
    flip = (pulseIn(PWM,HIGH)<2000);
    debugLog();
    if(flip!=lastState){
        lastChanged = millis();
        lastState = flip;
        logDuration = 0;
        if(debug){
        debugLog();
        }
    }
    debugLog();
    logDuration =  millis() - lastChanged;
    if(!flip){
        logDuration=0;
    }
    debugLog();
    if (flip) {
        debugLog();
        // get value, compose dataString
        val = scale.getScaleValue(1);
        val = val * (convertFactor)
        dataString = String(logDuration) + "," + String(val); // Build data string
        Serial.println(dataString);
        // write to sd & serial
        if (dataFile) {
            debugLog();
            dataFile.println( dataString );
            Serial.println(dataString);
        }
        else {
            Serial.println("error opening datalog.txt");
        }
    }
    debugLog();


}

void debugLog() {
    if(debug) {
        Serial.println("Switch state: " + String(flip)+", last state: "+ String(lastState));
        Serial.println("Time since last change: " + String(lastChanged));
        Serial.println("Current time: "+ String(millis()));
        Serial.println("logDuration: "+String(logDuration));
        Serial.println("Force: "+String(val));
        delay(200);
    }
}


double getScaleValue(int samplesize){
    // Gets direct value from sensor iff sample size == 1
    // Average can be taken if samsplesize > 1
    // requires scale object to exist
    // Return value is not scaled | modified in any way
    int val;
    if(samplesize == 1){
        val = scale.get_units(0);
        return val;
    }
    else{
        int count = 0;
        double avg = 0;
        for(int a = 0; a<sampleSize; a++){
            avg +=scale.get_units(0);
            count++;
        }
        avg /= count;
        return avg;
    }
}
