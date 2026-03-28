//STM32F4 - Data acquisition, valve & launch control
// ADS1256 cycleSingle() multi-channel acquisition
// SD card logging + compact JSON telemetry output

#include <ADS1256.h>
#include <Servo.h>
#include <STM32SD.h>
#include <SPI.h>

#define ARDUINO_ARCH_STM32

/*
 *  STM32F405 PIN ALLOCATION MAP
 *  =============================================
 *  [USED] SPI1 (ADS1256):    PA4(CS), PA5(SCK), PA6(MISO), PA7(MOSI), PA8(DRDY)
 *  [USED] UART1 (SERIAL):    PA9(TX), PA10(RX)
 *  [USED] SPI2 (SD CARD):    PB12(CS), PB13(SCK), PB14(MISO), PB15(MOSI)
 *  [USED] HX711:             PB6(DOUT), PB9(SCK)
 *
 *  PWM PINS:
 *  - TIM2: PA0, PA1, PA2, PA3 | TIM5: PC2, PC3 | TIM4: PB7 | TIM3: PB0
 *
 *  XBee FLOW CONTROL (optional, wire if packet loss is bad):
 *  - XBee CTS → STM32 PA11 (UART1_CTS)
 *  - XBee RTS → STM32 PA12 (UART1_RTS)
 *  Also set XBee firmware: D6=1 (RTS flow ctrl), D7=1 (CTS flow ctrl)
 */

// --- SERVO / PWM PIN DEFINITIONS ---
#define PIN_GSE_FILL    PA0   // TIM2_CH1
#define PIN_GSE_RELIEF  PA1   // TIM2_CH2
#define PIN_RKT_RELIEF  PA2   // TIM2_CH3
#define PIN_RKT_DUMP    PA3   // TIM2_CH4
#define PIN_RKT_IGN     PB0   // TIM3_CH3
#define PIN_RKT_FUEL    PC2   // TIM5_CH3
#define PIN_GSE_DUMP    PC3   // TIM5_CH4
#define PIN_RKT_OX      PB7   // TIM4_CH2

// --- SD CARD (SPI2) ---
#define PIN_SD_CS       PB12
// SCK/MISO/MOSI are PB13/PB14/PB15 — handled by STM32SD library

// --- HX711 LOAD CELL ---
#define PIN_HX711_DOUT   PB6
#define PIN_HX711_PD_SCK PB9

// --- ADS1256 (SPI1) ---
#if defined(USE_SPI2)
  SPIClass spi2(PB15, PB14, PB13);
  #define USE_SPI spi2
#else
  #define USE_SPI SPI   // PA7(MOSI), PA6(MISO), PA5(SCK)
#endif

ADS1256 A(PA8, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &USE_SPI);
//        DRDY, RESET,              SYNC(PDWN),           CS,  VREF

// ============ CONFIGURATION ============
const float MAX_VOLTAGE  = 5.0;
const float MAX_PRESSURE = 5000.0;
const float PRESSURE_OFFSETS[3] = {-40, -40, -40}; // PSI, one per channel

// How often to transmit over XBee (every N complete 3-channel cycles).
// At 2000 SPS / 3 channels ≈ 96 complete sets/sec.
// Set to 5 → ~19 Hz over radio (much easier on XBee).
// SD card always logs at full rate regardless of this value.
const int XBEE_OUTPUT_INTERVAL = 5;

// ============ SERIAL PORT ============
// #define USE_SERIAL_USB
#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1  // PA9(TX) / PA10(RX)
#endif
const long SERIAL_BAUD = 230400;

// ============ SERVO DEFINITIONS ============
Servo servo_gse_fill, servo_gse_relief, servo_gse_dump;
Servo servo_rkt_ox, servo_rkt_fuel, servo_rkt_relief, servo_rkt_dump, servo_rkt_ign;

#define SERVO_CLOSED 975
#define SERVO_OPEN   1900

// ============ VALVE STATES ============
int gse_fill_state = 0, gse_relief_state = 0, gse_dump_state = 0;
int rkt_ox_state = 0, rkt_fuel_state = 0, rkt_relief_state = 0;
int rkt_dump_state = 0, rkt_ign_state = 0;

// ============ ADC / PRESSURE GLOBALS ============
float bottlePressure = 0.0, tankPressure = 0.0, chamberPressure = 0.0;
long  rawBottle = 0, rawTank = 0, rawChamber = 0;
int   completeSetCounter = 0;
unsigned long loopCounter = 0;

// ============ SD CARD GLOBALS ============
bool  sdReady = false;
File  logFile;
char  logFileName[32];
unsigned long sdRowCount = 0;

// ============ SERIAL BUFFER ============
static String serialBuffer = "";

// ===================================================
// SD CARD — INITIALIZATION
// Opens a new numbered file (LOG_001.CSV, LOG_002.CSV…)
// so previous runs are never overwritten.
// ===================================================
void initSD() {
  // STM32SD needs the CS pin passed in; SPI2 pins are set at library level
  if (!SD.begin(PIN_SD_CS)) {
    SERIAL_PORT.println("SD: begin() failed — check wiring / card format (FAT32)");
    sdReady = false;
    return;
  }

  // Find the next available filename
  for (int i = 1; i <= 999; i++) {
    snprintf(logFileName, sizeof(logFileName), "LOG_%03d.CSV", i);
    if (!SD.exists(logFileName)) break;
  }

  logFile = SD.open(logFileName, FILE_WRITE);
  if (!logFile) {
    SERIAL_PORT.print("SD: could not open ");
    SERIAL_PORT.println(logFileName);
    sdReady = false;
    return;
  }

  // Write CSV header
  logFile.println("row,bottle_psi,tank_psi,chamber_psi,"
                  "gse_fill,gse_relief,gse_dump,"
                  "rkt_ox,rkt_fuel,rkt_relief,rkt_dump,rkt_ign");
  logFile.flush();

  sdReady = true;
  SERIAL_PORT.print("SD: logging to ");
  SERIAL_PORT.println(logFileName);
}

// ===================================================
// SD CARD — LOG ONE ROW
// Called on every complete 3-channel ADC cycle.
// Flush every 50 rows to limit write latency impact.
// ===================================================
void logToSD() {
  if (!sdReady) return;

  sdRowCount++;
  logFile.print(sdRowCount);       logFile.print(',');
  logFile.print((int)bottlePressure);  logFile.print(',');
  logFile.print((int)tankPressure);    logFile.print(',');
  logFile.print((int)chamberPressure); logFile.print(',');
  logFile.print(gse_fill_state);   logFile.print(',');
  logFile.print(gse_relief_state); logFile.print(',');
  logFile.print(gse_dump_state);   logFile.print(',');
  logFile.print(rkt_ox_state);     logFile.print(',');
  logFile.print(rkt_fuel_state);   logFile.print(',');
  logFile.print(rkt_relief_state); logFile.print(',');
  logFile.print(rkt_dump_state);   logFile.print(',');
  logFile.println(rkt_ign_state);

  // Flush periodically — every 50 rows ≈ every ~0.5s
  // Keeps data safe without killing ADC throughput
  if (sdRowCount % 50 == 0) {
    logFile.flush();
  }
}

// ===================================================
// VALVE CONTROL
// ===================================================
void setValve(Servo &servo, int &stateVar, int newState) {
  stateVar = newState;
  servo.writeMicroseconds(newState == 1 ? SERVO_OPEN : SERVO_CLOSED);
}

// ===================================================
// SERIAL COMMAND PARSER
// Called with a complete '\n'-terminated line.
// Compact key support: "f"=gse_fill, "gd"=gse_dump, etc.
// Also accepts the original long-form keys for compatibility.
// ===================================================
void handleSerialCommand(String &line) {
  if (line.indexOf("set_valve") == -1) return;

  // Robust state extraction
  int state = 0;
  int stateIdx = line.indexOf("\"state\":");
  if (stateIdx != -1) {
    int valIdx = stateIdx + 8;
    while (valIdx < (int)line.length() && line[valIdx] == ' ') valIdx++;
    state = (line[valIdx] == '1') ? 1 : 0;
  }

  // Match target — check short keys first, then long-form fallback
  if      (line.indexOf("\"gf\"")          != -1 || line.indexOf("gse_fill")     != -1) setValve(servo_gse_fill,    gse_fill_state,    state);
  else if (line.indexOf("\"gr\"")          != -1 || line.indexOf("gse_relief")   != -1) setValve(servo_gse_relief,  gse_relief_state,  state);
  else if (line.indexOf("\"gd\"")          != -1 || line.indexOf("gse_dump")     != -1) setValve(servo_gse_dump,    gse_dump_state,    state);
  else if (line.indexOf("\"ro\"")          != -1 || line.indexOf("rocket_ox")    != -1) setValve(servo_rkt_ox,      rkt_ox_state,      state);
  else if (line.indexOf("\"rf\"")          != -1 || line.indexOf("rocket_fuel")  != -1) setValve(servo_rkt_fuel,    rkt_fuel_state,    state);
  else if (line.indexOf("\"rr\"")          != -1 || line.indexOf("rocket_relief")!= -1) setValve(servo_rkt_relief,  rkt_relief_state,  state);
  else if (line.indexOf("\"rd\"")          != -1 || line.indexOf("rocket_dump")  != -1) setValve(servo_rkt_dump,    rkt_dump_state,    state);
  else if (line.indexOf("\"ig\"")          != -1 || line.indexOf("ignite")       != -1) setValve(servo_rkt_ign,     rkt_ign_state,     state);
}

// ===================================================
// PRESSURE CONVERSION
// ===================================================
float voltageToPressure(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > MAX_VOLTAGE) v = MAX_VOLTAGE;
  return (v / MAX_VOLTAGE) * MAX_PRESSURE;
}

// ===================================================
// SETUP
// ===================================================
void setup() {
  // --- Servos ---
  servo_gse_fill.attach(PIN_GSE_FILL);
  servo_gse_relief.attach(PIN_GSE_RELIEF);
  servo_gse_dump.attach(PIN_GSE_DUMP);
  servo_rkt_ox.attach(PIN_RKT_OX);
  servo_rkt_fuel.attach(PIN_RKT_FUEL);
  servo_rkt_relief.attach(PIN_RKT_RELIEF);
  servo_rkt_dump.attach(PIN_RKT_DUMP);
  servo_rkt_ign.attach(PIN_RKT_IGN);

  servo_gse_fill.writeMicroseconds(SERVO_CLOSED);
  servo_gse_relief.writeMicroseconds(SERVO_CLOSED);
  servo_gse_dump.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_ox.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_fuel.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_relief.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_dump.writeMicroseconds(SERVO_CLOSED);
  servo_rkt_ign.writeMicroseconds(SERVO_CLOSED);

  // --- Serial ---
  SERIAL_PORT.begin(SERIAL_BAUD);
  while (!SERIAL_PORT && millis() < 5000);

  // --- ADC ---
  A.InitializeADC();
  A.setPGA(PGA_1);
  A.setBuffer(0);
  A.setDRATE(DRATE_2000SPS);
  delay(100);
  A.sendDirectCommand(SELFCAL);
  delay(200);

  // --- SD Card ---
  initSD();

  SERIAL_PORT.println("Ready.");
}

// ===================================================
// LOOP
// ===================================================
void loop() {

  // --- Non-blocking serial read ---
  while (SERIAL_PORT.available()) {
    char c = SERIAL_PORT.read();
    if (c == '\n') {
      handleSerialCommand(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }

  // --- ADC cycling ---
  const uint8_t channels[3] = {SING_0, SING_1, SING_2};
  ADS1256::ADS1256_Result res = A.cycleSingle3Tracked(channels, 3);

  if (res.channel == 0xFF) return;  // Invalid / first sample

  loopCounter++;

  if      (res.channel == SING_2) rawBottle  = res.value;
  else if (res.channel == SING_0) rawTank    = res.value;
  else if (res.channel == SING_1) {
    rawChamber = res.value;
    completeSetCounter++;

    // --- Convert ---
    bottlePressure  = voltageToPressure(A.convertToVoltage(rawBottle))  + PRESSURE_OFFSETS[0];
    tankPressure    = voltageToPressure(A.convertToVoltage(rawTank))    + PRESSURE_OFFSETS[1];
    chamberPressure = voltageToPressure(A.convertToVoltage(rawChamber)) + PRESSURE_OFFSETS[2];

    // --- SD log every cycle (full rate) ---
    logToSD();

    // --- XBee transmit at reduced rate ---
    // Compact JSON: short keys keep payload ~80 bytes vs ~150 bytes
    // Full key compat is maintained in handleSerialCommand() above
    if (completeSetCounter % XBEE_OUTPUT_INTERVAL == 0) {
      // {"p":{"b":0,"t":0,"c":0},"g":{"l":0.00,"f":0,"r":0,"d":0},"r":{"o":0,"f":0,"r":0,"d":0,"i":0}}
      SERIAL_PORT.print(F("{\"p\":{\"b\":"));
      SERIAL_PORT.print((int)bottlePressure);
      SERIAL_PORT.print(F(",\"t\":"));
      SERIAL_PORT.print((int)tankPressure);
      SERIAL_PORT.print(F(",\"c\":"));
      SERIAL_PORT.print((int)chamberPressure);
      SERIAL_PORT.print(F("},\"g\":{\"l\":0,\"f\":"));
      SERIAL_PORT.print(gse_fill_state);
      SERIAL_PORT.print(F(",\"r\":"));
      SERIAL_PORT.print(gse_relief_state);
      SERIAL_PORT.print(F(",\"d\":"));
      SERIAL_PORT.print(gse_dump_state);
      SERIAL_PORT.print(F("},\"r\":{\"o\":"));
      SERIAL_PORT.print(rkt_ox_state);
      SERIAL_PORT.print(F(",\"f\":"));
      SERIAL_PORT.print(rkt_fuel_state);
      SERIAL_PORT.print(F(",\"r\":"));
      SERIAL_PORT.print(rkt_relief_state);
      SERIAL_PORT.print(F(",\"d\":"));
      SERIAL_PORT.print(rkt_dump_state);
      SERIAL_PORT.print(F(",\"i\":"));
      SERIAL_PORT.print(rkt_ign_state);
      SERIAL_PORT.println(F("}}"));
    }
  }
}