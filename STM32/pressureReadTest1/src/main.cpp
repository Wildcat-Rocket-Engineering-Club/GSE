//STM32F4 Nucleo - ADS1256 Pressure Logging System
//Reads 3 pressure transducers and outputs JSON-formatted data

#include <ADS1256.h>

#define ARDUINO_ARCH_STM32

// int drateValues[16] = {
//   DRATE_30000SPS,
//   DRATE_15000SPS,
//   DRATE_7500SPS,
//   DRATE_3750SPS,
//   DRATE_2000SPS,
//   DRATE_1000SPS,
//   DRATE_500SPS,
//   DRATE_100SPS,
//   DRATE_60SPS,
//   DRATE_50SPS,
//   DRATE_30SPS,
//   DRATE_25SPS,
//   DRATE_15SPS,
//   DRATE_10SPS,
//   DRATE_5SPS,
//   DRATE_2SPS
// };  //Array to store the sampling rates


#if defined(ARDUINO_ARCH_STM32)
#pragma message "Using STM32"
//#define USE_SPI2  //Uncomment to use SPI2
#if defined(USE_SPI2)
#pragma message "Using SPI2"
#define USE_SPI spi2
SPIClass spi2(PB15, PB14, PB13);  //MOSI, MISO, SCK
#else
#pragma message "Using SPI (SPI1)"
#define USE_SPI SPI  //Default SPI1, pre-instantiated as 'SPI' on PA7, PA6, PA5
#endif


//-----------------------------------------
#else  //Default fallback (Arduino AVR)
#define SPI_MOSI MOSI
#define SPI_MISO MISO
#define SPI_SCK SCK
#define USE_SPI SPI
//-----------------------------------------
#endif

// STM32F4 Nucleo pin configuration
ADS1256 A(PA8, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &USE_SPI); 
//DRDY, RESET, SYNC(PDWN), CS, VREF(float). //STM32 - SPI1

// ============ CONFIGURATION ============
// Voltage to Pressure conversion range: 0-2.5V maps to 0-5000 psi
const float MIN_VOLTAGE = 0.0;      // Volts
const float MAX_VOLTAGE = 5;      // Volts
const float MIN_PRESSURE = 0.0;     // PSI
const float MAX_PRESSURE = 5000.0;  // PSI

// Output frequency - adjust this to change readings per second
// 200ms = 5 readings/sec, 100ms = 10 readings/sec, 1000ms = 1 reading/sec
const int OUTPUT_INTERVAL_MS = 200;

// Channel mapping - single-ended channels
const int BOTTLE_PRESSURE_CHANNEL = SING_0;   // AIN0
const int TANK_PRESSURE_CHANNEL = SING_1;     // AIN1
const int CHAMBER_PRESSURE_CHANNEL = SING_2;  // AIN2

// ============ GLOBAL VARIABLES ============
float bottlePressure = 0.0;
float tankPressure = 0.0;
float chamberPressure = 0.0;

// Utility function to convert voltage to pressure
float voltageToPressure(float voltage) {
  // Linear conversion: 0-2.5V -> 0-5000 psi
  if (voltage < MIN_VOLTAGE) voltage = MIN_VOLTAGE;
  if (voltage > MAX_VOLTAGE) voltage = MAX_VOLTAGE;
  
  float normalized = voltage / MAX_VOLTAGE;
  return normalized * MAX_PRESSURE;
}

// Function to read a single channel and return the voltage
float readChannel(int channel) {
  A.setMUX(channel);
  delayMicroseconds(20000);  // Increased settling time for channel switch (20ms at 100 SPS)
  long raw = A.readSingle();
  float voltage = A.convertToVoltage(raw);
  
  // // Debug: print raw ADC value occasionally
  // static int debugCounter = 0;
  // debugCounter++;
  // if (debugCounter >= 10) {  // Print every 10 calls
  //   SERIAL_PORT.print("Channel ");
  //   SERIAL_PORT.print(channel);
  //   SERIAL_PORT.print(" raw: ");
  //   SERIAL_PORT.print(raw);
  //   SERIAL_PORT.print(", voltage: ");
  //   SERIAL_PORT.println(voltage, 4);
  //   debugCounter = 0;
  // }
  
  return voltage;
}

// ============ SERIAL PORT CONFIGURATION ============
// Uncomment the line below to use USB Serial (Serial)
// Comment it out to use Serial1 (hardware TX=PA9, RX=PA10)
//#define USE_SERIAL_USB

#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1  // Hardware UART on PA9(TX)/PA10(RX)
#endif

// Change baud rate for performance
const long SERIAL_BAUD = 9600; 

// ============ TIMING VARIABLES ============
unsigned long lastOutputTime = 0;

void setup() {
  SERIAL_PORT.begin(SERIAL_BAUD); // Match this in your Serial Monitor!
  
  while (!SERIAL_PORT && millis() < 5000);

  A.InitializeADC();
  A.setPGA(PGA_1);  // PGA_0 for ±2.5V range (correct for 0-2.5V inputs)
  A.setBuffer(0);  // Disable input buffer to reduce noise from high impedance sources
  A.setDRATE(DRATE_100SPS); // Lower data rate for better stability with high impedance sources
  delay(100); // Allow settings to settle
  
  // Perform self-calibration
  A.sendDirectCommand(SELFCAL);
  delay(200); // Wait for calibration to complete
  
  SERIAL_PORT.println("✓ ADC calibrated and ready");
}

void loop() {
  unsigned long start = micros();
  // 1. ALWAYS READ (High Speed Acquisition)
  // We read every loop so the internal filter stays "warm"
  
  float bottleVoltage = readChannel(BOTTLE_PRESSURE_CHANNEL);
  float tankVoltage   = readChannel(TANK_PRESSURE_CHANNEL);
  float chamberVoltage= readChannel(CHAMBER_PRESSURE_CHANNEL);

  // Convert to pressure
  bottlePressure = voltageToPressure(bottleVoltage);
  tankPressure   = voltageToPressure(tankVoltage);
  chamberPressure = voltageToPressure(chamberVoltage);

  // // Debug: Print raw voltages occasionally
  // static int debugCounter = 0;
  // debugCounter++;
  // if (debugCounter >= 50) {  // Print every 50 loops
  //   SERIAL_PORT.print("DEBUG - Voltages: Bottle=");
  //   SERIAL_PORT.print(bottleVoltage, 4);
  //   SERIAL_PORT.print("V, Tank=");
  //   SERIAL_PORT.print(tankVoltage, 4);
  //   SERIAL_PORT.print("V, Chamber=");
  //   SERIAL_PORT.print(chamberVoltage, 4);
  //   SERIAL_PORT.println("V");
  //   debugCounter = 0;
  // }

  // 2. NON-BLOCKING OUTPUT
  // Check if it's time to send data without using delay()
  unsigned long currentTime = millis();
  if (currentTime - lastOutputTime >= OUTPUT_INTERVAL_MS) {
    lastOutputTime = currentTime;

    // Use a single Print if possible to reduce overhead
    SERIAL_PORT.print("{\"pressure_transducers\":{\"bottle_pressure\":");
    SERIAL_PORT.print((int)bottlePressure);
    SERIAL_PORT.print(",\"tank_pressure\":");
    SERIAL_PORT.print((int)tankPressure);
    SERIAL_PORT.print(",\"chamber_pressure\":");
    SERIAL_PORT.print((int)chamberPressure);
    SERIAL_PORT.println("},\"gse\":{\"loadcell\":0.00,\"fill\":0,\"pyro\":0,\"relief\":0},\"rocket\":{\"pyro\":0,\"ox\":0,\"fuel\":0,\"relief\":0}}");
    unsigned long elapsed = micros() - start;
    SERIAL_PORT.print(" // loop: ");
    SERIAL_PORT.print(elapsed);
    SERIAL_PORT.println(" us");  }
}