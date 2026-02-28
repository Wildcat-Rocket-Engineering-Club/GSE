#include "HX711.h"
#include <SPI.h>
#include <SD.h>

bool debug = false;
int dataPin = 2;
int clockPin = 3;
int PWM = 4;
int chipSelect = 10;
const double convertFactor =-0.00232083132251;
int samples = 1;
//int timer;
//uint32_t start, stop;

bool flip, lastState;
long lastChanged = 0;
volatile double val;
HX711 scale;
File dataFile;
String dataString;
String fileName;
// variables used for button toggle debounce
unsigned long logDuration = 0; // millis
double getScaleValue(int samplesize);

void setup() {
    pinMode(PWM, INPUT);
    Serial.begin(9600);
    // Check SD card initialization
    String filename_base = "datalog_";
    String file_extension = ".csv";
    int file_num = 0;
    // initialize SPI
    if (!SD.begin(chipSelect)) {
        Serial.println("SD card initialization failed!");
        while(1);
    }
    Serial.println("SD card initialized.");
    // find next available file name
    while(SD.exists(filename_base + file_num + file_extension)){
        file_num++;
    }
    fileName = (String) filename_base + file_num + file_extension;
    dataFile = SD.open(fileName, FILE_WRITE);
     // initialize csv header
    dataFile.println("Time (s),Force (N)");
    if (dataFile) {
            debugLog();
            Serial.println("File Created");
        }
        else {
            Serial.println("error opening "+fileName);
        }
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
        val = getScaleValue(samples);
        val = val * (convertFactor);
        dataString = String(logDuration) + "," + String(val); // Build data string
        Serial.println(dataString);
        // write to sd & serial
        if (dataFile) {
            debugLog();
            dataFile.println( dataString );
            Serial.println(dataString);
        }
        else {
            Serial.println("error opening "+fileName);
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
        delay(1000);
    }
}


double getScaleValue(int samplesize){
    // Gets direct value from sensor iff sample size == 1
    // Average can be taken if samsplesize > 1
    // requires scale object to exist
    // Return value is not scaled | modified in any way
    int val;
    if(samplesize == 1){
        return scale.get_units(0);
    }
    else{
        int count = 0;
        double avg = 0;
        for(int a = 0; a<samplesize; a++){
            avg +=scale.get_units(0);
            count++;
        }
        avg /= count;
        return avg;
    }
}
