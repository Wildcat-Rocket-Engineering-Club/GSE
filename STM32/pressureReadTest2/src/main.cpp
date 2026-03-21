//STM32F4 Nucleo - ADS1256 Pressure Logging System (OPTIMIZED / v2)
//Uses cycleSingle() for faster multi-channel acquisition
//Expected: ~96 complete 3-channel reads per second

#include <ADS1256.h>

#define ARDUINO_ARCH_STM32

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

#else  //Default fallback (Arduino AVR)
#define SPI_MOSI MOSI
#define SPI_MISO MISO
#define SPI_SCK SCK
#define USE_SPI SPI
#endif

// STM32F4 Nucleo pin configuration
ADS1256 A(PA8, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &USE_SPI); 
//        DRDY,RESET,               SYNC(PDWN),           CS, VREF

// ============ CONFIGURATION ============
// Voltage to Pressure conversion range: 0-2.5V maps to 0-5000 psi
const float MIN_VOLTAGE = 0.0;     // Volts
const float MAX_VOLTAGE = 5.0;     // Volts

const float MIN_PRESSURE = 0.0;       // PSI
const float MAX_PRESSURE = 5000.0;    // PSI

const float PRESSURE_OFFSETS[3] = {-2, 0, 0}; // PSI

// Output frequency - every N complete 3-channel sets
// 1 = output every complete cycle, 10 = output every 10 cycles, etc.
const int OUTPUT_INTERVAL_CYCLES = 1;

// ============ GLOBAL VARIABLES ============
float bottlePressure = 0.0;
float tankPressure = 0.0;
float chamberPressure = 0.0;

long rawBottle = 0;
long rawTank = 0;
long rawChamber = 0;

int cycleCounter = 0;           // Tracks position in cycleSingle3 rotation (0, 1, 2)
int completeSetCounter = 0;     // Counts complete 3-channel sets
unsigned long loopCounter = 0;  // Total loops for diagnostics

// ============ SERIAL PORT CONFIGURATION ============
#define USE_SERIAL_USB

#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1  // Hardware UART on PA9(TX)/PA10(RX)
#endif

const long SERIAL_BAUD = 115200;

// Utility function to convert voltage to pressure
float voltageToPressure(float voltage) {
  if (voltage < MIN_VOLTAGE) voltage = MIN_VOLTAGE;
  if (voltage > MAX_VOLTAGE) voltage = MAX_VOLTAGE;
  
  float normalized = voltage / MAX_VOLTAGE;
  return normalized * MAX_PRESSURE;
}

void setup() {
  SERIAL_PORT.begin(SERIAL_BAUD);
  
  while (!SERIAL_PORT && millis() < 5000);

  A.InitializeADC();
  A.setPGA(PGA_1);
  A.setBuffer(0);
  A.setDRATE(DRATE_2000SPS);  // Use high data rate for fast cycling
  delay(100);
  
  A.sendDirectCommand(SELFCAL);
  delay(200);
  
  SERIAL_PORT.println("✓ ADC initialized (cycleSingle mode)");
  SERIAL_PORT.println("Expected: ~96-100 3-channel reads/sec");
  delay(500);
}

void loop() {
const uint8_t channels[3] = {SING_0, SING_1, SING_2};
    ADS1256::ADS1256_Result res = A.cycleSingle3Tracked(channels, 3);

  // Ignore invalid first sample
  if (res.channel == 0xFF) return;

  loopCounter++;

  // Assign based on ACTUAL channel (no guessing!)
  if (res.channel == SING_2) {
    rawBottle = res.value;
  }
  else if (res.channel == SING_0) {
    rawTank = res.value;
  }
  else if (res.channel == SING_1) {
    rawChamber = res.value;

    // Only process once we hit last channel
    completeSetCounter++;

    float bottleVoltage = A.convertToVoltage(rawBottle);
    float tankVoltage = A.convertToVoltage(rawTank);
    float chamberVoltage = A.convertToVoltage(rawChamber);

    bottlePressure = voltageToPressure(bottleVoltage) + PRESSURE_OFFSETS[0];
    tankPressure = voltageToPressure(tankVoltage) + PRESSURE_OFFSETS[1];
    chamberPressure = voltageToPressure(chamberVoltage) + PRESSURE_OFFSETS[2];

    if (completeSetCounter % OUTPUT_INTERVAL_CYCLES == 0) {
      SERIAL_PORT.print("{\"pressure_transducers\":{\"bottle_pressure\":");
      SERIAL_PORT.print((int)bottlePressure);
      SERIAL_PORT.print(",\"tank_pressure\":");
      SERIAL_PORT.print((int)tankPressure);
      SERIAL_PORT.print(",\"chamber_pressure\":");
      SERIAL_PORT.print((int)chamberPressure);
      SERIAL_PORT.println("},\"gse\":{\"loadcell\":0.00,\"fill\":0,\"pyro\":0,\"relief\":0},\"rocket\":{\"pyro\":0,\"ox\":0,\"fuel\":0,\"relief\":0}}");
    }
  }
}