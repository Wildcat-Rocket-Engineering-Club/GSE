//STM32F4 Nucleo - ADS1256 Pressure Logging System (OPTIMIZED / v2)
//Uses cycleSingle() for faster multi-channel acquisition
//Expected: ~96 complete 3-channel reads per second

#include <ADS1256.h>
#include <Servo.h>

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

// ==============================
// SERVO DEFINITIONS
// ==============================

Servo servo_gse_fill;
Servo servo_gse_relief;
Servo servo_gse_dump;

Servo servo_rkt_ox;
Servo servo_rkt_fuel;
Servo servo_rkt_relief;
Servo servo_rkt_dump;
Servo servo_rkt_ign;

// ==============================
// SERVO PINS (CHANGE THESE)
// ==============================

#define PIN_GSE_FILL    PA0
#define PIN_GSE_RELIEF  PA1
#define PIN_GSE_DUMP    PA2

#define PIN_RKT_OX      PA3
#define PIN_RKT_FUEL    PA5
#define PIN_RKT_RELIEF  PA6
#define PIN_RKT_DUMP    PA7
#define PIN_RKT_IGN     PB0

// ==============================
// VALVE STATES
// ==============================

int gse_fill_state = 0;
int gse_relief_state = 0;
int gse_dump_state = 0;

int rkt_ox_state = 0;
int rkt_fuel_state = 0;
int rkt_relief_state = 0;
int rkt_dump_state = 0;
int rkt_ign_state = 0;


// ==============================
// PWM VALUES (TUNE THESE)
// ==============================

#define SERVO_CLOSED 1100
#define SERVO_OPEN   1900

// ============ SERIAL PORT CONFIGURATION ============
//#define USE_SERIAL_USB

#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1  // Hardware UART on PA9(TX)/PA10(RX)
#endif

const long SERIAL_BAUD = 230400;

// Utility function to set any valve (or the ignition)
void setValve(Servo &servo, int &stateVar, int newState) {
  stateVar = newState;

  if (newState == 1) {
    servo.writeMicroseconds(SERVO_OPEN);
  } else {
    servo.writeMicroseconds(SERVO_CLOSED);
  }
}

// Utility to handle receipt of serial commands
void handleSerialCommand(String line) {

  if (line.indexOf("set_valve") == -1) return;

  // More robust state detection
  int state = 0;
  int stateIdx = line.indexOf("\"state\":");
  if (stateIdx != -1) {
      // Skip past "state": and any spaces
      int valIdx = stateIdx + 8;
      while (valIdx < line.length() && line[valIdx] == ' ') valIdx++;
      state = (line[valIdx] == '1') ? 1 : 0;
  }

  // GSE
  if (line.indexOf("gse_fill") != -1) {
    setValve(servo_gse_fill, gse_fill_state, state);
  }
  else if (line.indexOf("gse_relief") != -1) {
    setValve(servo_gse_relief, gse_relief_state, state);
  }
  else if (line.indexOf("gse_dump") != -1) {
    setValve(servo_gse_dump, gse_dump_state, state);
  }

  // ROCKET
  else if (line.indexOf("rocket_ox") != -1) {
    setValve(servo_rkt_ox, rkt_ox_state, state);
  }
  else if (line.indexOf("rocket_fuel") != -1) {
    setValve(servo_rkt_fuel, rkt_fuel_state, state);
  }
  else if (line.indexOf("rocket_relief") != -1) {
    setValve(servo_rkt_relief, rkt_relief_state, state);
  }
  else if (line.indexOf("rocket_dump") != -1) {
    setValve(servo_rkt_dump, rkt_dump_state, state);
  }
  else if (line.indexOf("ignite") != -1) {
    setValve(servo_rkt_ign, rkt_ign_state, state);
  }
}



// Utility function to convert voltage to pressure
float voltageToPressure(float voltage) {
  if (voltage < MIN_VOLTAGE) voltage = MIN_VOLTAGE;
  if (voltage > MAX_VOLTAGE) voltage = MAX_VOLTAGE;
  
  float normalized = voltage / MAX_VOLTAGE;
  return normalized * MAX_PRESSURE;
}

void setup() {

  // ==============================
  // SERVO INIT
  // ==============================

  servo_gse_fill.attach(PIN_GSE_FILL);
  servo_gse_relief.attach(PIN_GSE_RELIEF);
  servo_gse_dump.attach(PIN_GSE_DUMP);

  servo_rkt_ox.attach(PIN_RKT_OX);
  servo_rkt_fuel.attach(PIN_RKT_FUEL);
  servo_rkt_relief.attach(PIN_RKT_RELIEF);
  servo_rkt_dump.attach(PIN_RKT_DUMP);
  servo_rkt_ign.attach(PIN_RKT_IGN);

  // Initialize all to CLOSED
  servo_gse_fill.writeMicroseconds(SERVO_CLOSED);
  servo_gse_relief.writeMicroseconds(SERVO_CLOSED);
  servo_gse_dump.writeMicroseconds(SERVO_CLOSED);

  servo_rkt_ox.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_fuel.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_relief.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_dump.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_ign.writeMicroseconds(SERVO_CLOSED);

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

static String serialBuffer = "";

while (SERIAL_PORT.available()) {
    char c = SERIAL_PORT.read();
    if (c == '\n') {
        handleSerialCommand(serialBuffer);
        serialBuffer = "";
    } else {
        serialBuffer += c;
    }
}
  
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
      SERIAL_PORT.print("},\"gse\":{\"loadcell\":0.00,\"fill\":");
      SERIAL_PORT.print(gse_fill_state);
      SERIAL_PORT.print(",\"relief\":");
      SERIAL_PORT.print(gse_relief_state);
      SERIAL_PORT.print(",\"dump\":");
      SERIAL_PORT.print(gse_dump_state);

      SERIAL_PORT.print("},\"rocket\":{\"ox\":");
      SERIAL_PORT.print(rkt_ox_state);
      SERIAL_PORT.print(",\"fuel\":");
      SERIAL_PORT.print(rkt_fuel_state);
      SERIAL_PORT.print(",\"relief\":");
      SERIAL_PORT.print(rkt_relief_state);
      SERIAL_PORT.print(",\"dump\":");
      SERIAL_PORT.print(rkt_dump_state);
      SERIAL_PORT.print(",\"ign\":");
      SERIAL_PORT.print(rkt_ign_state);

      SERIAL_PORT.println("}}");
    }
  }
}