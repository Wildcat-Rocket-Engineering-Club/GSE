//STM32F4 Nucleo - ADS1256 Pressure Logging System
//Reads 3 pressure transducers and outputs JSON-formatted data

#include <ADS1256.h>

#define ARDUINO_ARCH_STM32

//Platform-specific pin definitions
#if defined(ARDUINO_ARCH_RP2040)
#pragma message "Using RP2040"
//#define USE_SPI1 //Alternative USE_SPI for RP2040 - Uncomment to use SPI1
#if defined(USE_SPI1)
#pragma message "Using SPI1 (SPI1)"
#define SPI_MOSI 11
#define SPI_MISO 12
#define SPI_SCK 10
#define USE_SPI SPI1
#else
#pragma message "Using SPI (SPI0)"
#define SPI_MOSI 3  //19
#define SPI_MISO 4  //16
#define SPI_SCK 2   //18
#define USE_SPI SPI
#endif
//-----------------------------------------

#elif defined(ARDUINO_ARCH_STM32)
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

#elif defined(TEENSYDUINO)
#pragma message "Using Teensy"
//#define USE_SPI1 //Uncomment to use SPI1 on Teensy 4.0 or 4.1
//#define USE_SPI2 //Uncomment to use SPI2 on Teensy 4.0 or 4.1
#if defined(USE_SPI2)
#pragma message "Using SPI2 (SPI3)"
#define USE_SPI SPI2
#elif defined(USE_SPI1)
#pragma message "Using SPI1 (SPI2)"
#define USE_SPI SPI1
#else
#pragma message "Using SPI (SPI1)"
#define USE_SPI SPI
#endif
//-----------------------------------------

#elif defined(ARDUINO_ARCH_ESP32)
#pragma message "Using ESP32"
SPIClass hspi(HSPI);
//#define USE_HSPI  // Uncomment to use HSPI instead of VSPI
#if defined(USE_HSPI)
#pragma message "Using HSPI"
#define USE_SPI hspi
#else
#pragma message "Using VSPI"
#define USE_SPI SPI
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
const float MAX_VOLTAGE = 2.5;      // Volts
const float MIN_PRESSURE = 0.0;     // PSI
const float MAX_PRESSURE = 5000.0;  // PSI

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
  delay(10);  // Small delay for channel settling
  return A.convertToVoltage(A.readSingle());
}

void setup() {
  Serial.begin(115200);  // Higher baud rate for serial efficiency
  
  while (!Serial) {
    ;  // Wait until the serial becomes available
  }

  // Startup status message
  Serial.println("\n================================");
  Serial.println("STM32F4 Pressure Logging System");
  Serial.println("================================");
  Serial.println("Initializing ADS1256...");

#if defined(ARDUINO_ARCH_RP2040)
  SPI.setSCK(SPI_SCK);
  SPI.setTX(SPI_MOSI);
  SPI.setRX(SPI_MISO);
#endif

#if defined(USE_HSPI)
  hspi.begin(14, 25, 13);  //SCK, MISO (safe), MOSI
#endif

  A.InitializeADC();
  Serial.println("ADS1256 initialized");
  
  // Configure the ADC
  A.setPGA(PGA_1);
  A.setDRATE(DRATE_500SPS);
  
  Serial.println("ADC configured:");
  Serial.println("  - PGA: 1x");
  Serial.println("  - Sample Rate: 500 SPS");
  Serial.println("  - Voltage Range: 0-2.5V");
  Serial.println("  - Pressure Range: 0-5000 psi");
  Serial.println("\nChannels:");
  Serial.println("  - Ch0 (AIN0): Bottle Pressure");
  Serial.println("  - Ch1 (AIN1): Tank Pressure");
  Serial.println("  - Ch2 (AIN2): Chamber Pressure");
  Serial.println("\nLogging started (1 second delay)...");
  Serial.println("================================\n");
  
  delay(1000);
}
int packetDelay = 1000; // Delay between readings and sending packets
int readDelay = 5; // Delay between reading individual channels
bool doReadDelay = true;

void loop() {
  // Read all three pressure channels
  A.setMUX(BOTTLE_PRESSURE_CHANNEL);

  if (doReadDelay)
  {
    delay(readDelay);
  }

  float bottleVoltage = A.convertToVoltage(A.readSingle());
  bottlePressure = voltageToPressure(bottleVoltage);
  
  A.setMUX(TANK_PRESSURE_CHANNEL);

  if (doReadDelay)
  {
    delay(readDelay);
  }

  float tankVoltage = A.convertToVoltage(A.readSingle());
  tankPressure = voltageToPressure(tankVoltage);
  
  A.setMUX(CHAMBER_PRESSURE_CHANNEL);

  if (doReadDelay)
  {
    delay(readDelay);
  }

  float chamberVoltage = A.convertToVoltage(A.readSingle());
  chamberPressure = voltageToPressure(chamberVoltage);
  
  // Output JSON formatted data
  Serial.print("{");
  Serial.print("\"pressure_transducers\":{");
  Serial.print("\"bottle_pressure\":");
  Serial.print((int)bottlePressure);
  Serial.print(",\"tank_pressure\":");
  Serial.print((int)tankPressure);
  Serial.print(",\"chamber_pressure\":");
  Serial.print((int)chamberPressure);
  Serial.print("},\"gse\":{");
  Serial.print("\"loadcell\":0.00,\"fill\":0,\"pyro\":0,\"relief\":0");
  Serial.print("},\"rocket\":{");
  Serial.print("\"pyro\":0,\"ox\":0,\"fuel\":0,\"relief\":0");
  Serial.println("}}");
  
  delay(packetDelay);  // Delay between full readings
}