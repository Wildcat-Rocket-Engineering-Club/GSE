//STM32F4 - Data acquisition, valve & launch control
// ADS1256 cycleSingle() multi-channel acquisition
// SD card logging (SdFat by Greiman) + compact JSON telemetry output

#include <ADS1256.h>   // Modified ADS1256 library
#include <Servo.h>     // Servo PWM control
#include <SdFat.h>     // SdFat by Bill Greiman (greiman/SdFat @ ^2.2.0)
#include <SPI.h>

#define ARDUINO_ARCH_STM32

/*
 *  STM32F405 PIN ALLOCATION MAP
 *  =============================================
 *  [USED] SPI1 (ADS1256):    PA4(CS), PA5(SCK), PA6(MISO), PA7(MOSI), PA8(DRDY)
 *  [USED] SPI2 (SD CARD):    PB12(CS), PB13(SCK), PB14(MISO), PB15(MOSI)
 *  [USED] UART1 (SERIAL):    PA9(TX), PA10(RX)
 *  [USED] HX711:             PB6(DOUT), PB9(SCK)
 *
 *  PWM PINS:
 *  - TIM2: PA0, PA1, PA2, PA3
 *  - TIM5: PC2, PC3
 *  - TIM4: PB7
 *  - TIM3: PB0
 *
 *  XBee FLOW CONTROL (wire if packet loss persists):
 *  - XBee CTS → STM32 PA11 (UART1_CTS)
 *  - XBee RTS → STM32 PA12 (UART1_RTS)
 *  Also set XBee firmware: D6=1 (RTS), D7=1 (CTS)
 */

// ============================================================
// PIN DEFINITIONS
// ============================================================

// --- Servo / PWM ---
#define PIN_GSE_FILL    PA0   // TIM2_CH1
#define PIN_GSE_RELIEF  PA1   // TIM2_CH2
#define PIN_RKT_RELIEF  PA2   // TIM2_CH3
#define PIN_RKT_DUMP    PA3   // TIM2_CH4
#define PIN_RKT_IGN     PB0   // TIM3_CH3
#define PIN_RKT_FUEL    PC2   // TIM5_CH3
#define PIN_GSE_DUMP    PC3   // TIM5_CH4
#define PIN_RKT_OX      PB7   // TIM4_CH2

// --- SD Card (SPI2) ---
#define PIN_SD_CS       PB12
#define PIN_SD_SCK      PB13
#define PIN_SD_MISO     PB14
#define PIN_SD_MOSI     PB15

// --- HX711 Load Cell ---
#define PIN_HX711_DOUT   PB6
#define PIN_HX711_PD_SCK PB9

// ============================================================
// SPI BUS SETUP
// SPI1 = ADS1256 (default Arduino SPI on PA5/PA6/PA7)
// SPI2 = SD card (PB13/PB14/PB15)
// Both are independent hardware peripherals — no contention.
// ============================================================

SPIClass spi2(PIN_SD_MOSI, PIN_SD_MISO, PIN_SD_SCK);  // SPI2 for SD card

// ADS1256 on SPI1 (the default 'SPI' instance on PA5/PA6/PA7)
ADS1256 A(PA8, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &SPI);
//        DRDY, RESET,              SYNC(PDWN),           CS,  VREF

// ============================================================
// SD CARD (SdFat)
// ============================================================

SdFat  sd;
SdFile logFile;

// ============================================================
// CONFIGURATION
// ============================================================

const float MAX_VOLTAGE  = 5.0f;
const float MAX_PRESSURE = 5000.0f;
const float PRESSURE_OFFSETS[3] = {-40.0f, -40.0f, -40.0f};  // PSI per channel

// XBee transmit throttle: send every N complete ADC cycles.
// At ~96 cycles/sec, 5 → ~19 Hz over radio.
// SD card always logs at full rate regardless.
const int XBEE_OUTPUT_INTERVAL = 5;

// ============================================================
// SERIAL PORT
// ============================================================

// #define USE_SERIAL_USB   // Uncomment to use USB CDC instead of UART
#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1   // PA9(TX) / PA10(RX)
#endif

const long SERIAL_BAUD = 230400;

// ============================================================
// SERVO OBJECTS & STATES
// ============================================================

Servo servo_gse_fill, servo_gse_relief, servo_gse_dump;
Servo servo_rkt_ox, servo_rkt_fuel, servo_rkt_relief, servo_rkt_dump, servo_rkt_ign;

#define SERVO_CLOSED 975
#define SERVO_OPEN   1900

int gse_fill_state    = 0;
int gse_relief_state  = 0;
int gse_dump_state    = 0;
int rkt_ox_state      = 0;
int rkt_fuel_state    = 0;
int rkt_relief_state  = 0;
int rkt_dump_state    = 0;
int rkt_ign_state     = 0;

// ============================================================
// ADC / PRESSURE GLOBALS
// ============================================================

float bottlePressure  = 0.0f;
float tankPressure    = 0.0f;
float chamberPressure = 0.0f;

long rawBottle  = 0;
long rawTank    = 0;
long rawChamber = 0;

int           completeSetCounter = 0;
unsigned long loopCounter        = 0;

// ============================================================
// SD CARD GLOBALS
// ============================================================

bool          sdReady    = false;
char          logFileName[16];
unsigned long sdRowCount = 0;

// ============================================================
// SERIAL RECEIVE BUFFER
// ============================================================

static String serialBuffer = "";

// ============================================================
// SD CARD — INIT
// Creates a new auto-numbered file (LOG_001.CSV … LOG_999.CSV)
// so previous runs are never overwritten.
// ============================================================

void initSD() {
  spi2.begin();  // Bring up SPI2 before handing to SdFat

  SdSpiConfig cfg(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(18), &spi2);

  if (!sd.begin(cfg)) {
    SERIAL_PORT.println(F("SD: begin() failed — check wiring / FAT32 format"));
    sdReady = false;
    return;
  }

  // Find the next available filename
  for (int i = 1; i <= 999; i++) {
    snprintf(logFileName, sizeof(logFileName), "LOG_%03d.CSV", i);
    if (!sd.exists(logFileName)) break;
  }

  if (!logFile.open(logFileName, O_RDWR | O_CREAT | O_TRUNC)) {
    SERIAL_PORT.print(F("SD: could not open "));
    SERIAL_PORT.println(logFileName);
    sdReady = false;
    return;
  }

  // Write CSV header
  logFile.println(F("row,bottle_psi,tank_psi,chamber_psi,"
                    "gse_fill,gse_relief,gse_dump,"
                    "rkt_ox,rkt_fuel,rkt_relief,rkt_dump,rkt_ign"));
  logFile.sync();

  sdReady = true;
  SERIAL_PORT.print(F("SD: logging to "));
  SERIAL_PORT.println(logFileName);
}

// ============================================================
// SD CARD — LOG ONE ROW
// Called every complete 3-channel ADC cycle (full ~96 Hz rate).
// sync() every 50 rows (~0.5 s) to protect against power loss
// without stalling the ADC loop.
// ============================================================

void logToSD() {
  if (!sdReady) return;

  sdRowCount++;
  logFile.print(sdRowCount);            logFile.print(',');
  logFile.print((int)bottlePressure);   logFile.print(',');
  logFile.print((int)tankPressure);     logFile.print(',');
  logFile.print((int)chamberPressure);  logFile.print(',');
  logFile.print(gse_fill_state);        logFile.print(',');
  logFile.print(gse_relief_state);      logFile.print(',');
  logFile.print(gse_dump_state);        logFile.print(',');
  logFile.print(rkt_ox_state);          logFile.print(',');
  logFile.print(rkt_fuel_state);        logFile.print(',');
  logFile.print(rkt_relief_state);      logFile.print(',');
  logFile.print(rkt_dump_state);        logFile.print(',');
  logFile.println(rkt_ign_state);

  if (sdRowCount % 50 == 0) {
    logFile.sync();
  }
}

// ============================================================
// VALVE CONTROL
// ============================================================

void setValve(Servo &servo, int &stateVar, int newState) {
  stateVar = newState;
  servo.writeMicroseconds(newState == 1 ? SERVO_OPEN : SERVO_CLOSED);
}

// ============================================================
// SERIAL COMMAND PARSER
// Accepts both compact short keys and original long-form keys.
//
// Compact key map (matches app.py COMPACT_TARGET_MAP):
//   "gf" = gse_fill    "gr" = gse_relief  "gd" = gse_dump
//   "ro" = rocket_ox   "rf" = rocket_fuel  "rr" = rocket_relief
//   "rd" = rocket_dump "ig" = ignite
// ============================================================

void handleSerialCommand(String &line) {
  if (line.indexOf("set_valve") == -1) return;

  // Robust state extraction — handles spaces after colon
  int state    = 0;
  int stateIdx = line.indexOf("\"state\":");
  if (stateIdx != -1) {
    int valIdx = stateIdx + 8;
    while (valIdx < (int)line.length() && line[valIdx] == ' ') valIdx++;
    state = (line[valIdx] == '1') ? 1 : 0;
  }

  // Match target — short key first, long-key fallback
  if      (line.indexOf("\"gf\"")  != -1 || line.indexOf("gse_fill")      != -1) setValve(servo_gse_fill,   gse_fill_state,   state);
  else if (line.indexOf("\"gr\"")  != -1 || line.indexOf("gse_relief")    != -1) setValve(servo_gse_relief, gse_relief_state, state);
  else if (line.indexOf("\"gd\"")  != -1 || line.indexOf("gse_dump")      != -1) setValve(servo_gse_dump,   gse_dump_state,   state);
  else if (line.indexOf("\"ro\"")  != -1 || line.indexOf("rocket_ox")     != -1) setValve(servo_rkt_ox,     rkt_ox_state,     state);
  else if (line.indexOf("\"rf\"")  != -1 || line.indexOf("rocket_fuel")   != -1) setValve(servo_rkt_fuel,   rkt_fuel_state,   state);
  else if (line.indexOf("\"rr\"")  != -1 || line.indexOf("rocket_relief") != -1) setValve(servo_rkt_relief, rkt_relief_state, state);
  else if (line.indexOf("\"rd\"")  != -1 || line.indexOf("rocket_dump")   != -1) setValve(servo_rkt_dump,   rkt_dump_state,   state);
  else if (line.indexOf("\"ig\"")  != -1 || line.indexOf("ignite")        != -1) setValve(servo_rkt_ign,    rkt_ign_state,    state);
}

// ============================================================
// PRESSURE CONVERSION
// ============================================================

float voltageToPressure(float v) {
  if (v < 0.0f)        v = 0.0f;
  if (v > MAX_VOLTAGE) v = MAX_VOLTAGE;
  return (v / MAX_VOLTAGE) * MAX_PRESSURE;
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  // --- Attach & zero all servos ---
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

  // --- ADS1256 (SPI1) ---
  A.InitializeADC();
  A.setPGA(PGA_1);
  A.setBuffer(0);
  A.setDRATE(DRATE_2000SPS);
  delay(100);
  A.sendDirectCommand(SELFCAL);
  delay(200);

  // --- SD card (SPI2) ---
  initSD();

  SERIAL_PORT.print(F("Ready. SD="));
  SERIAL_PORT.println(sdReady ? logFileName : "FAIL");
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  // --- Non-blocking serial receive ---
  // One character at a time — ADC loop is never blocked waiting for serial.
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

  if (res.channel == 0xFF) return;  // Invalid / warmup sample — skip

  loopCounter++;

  if      (res.channel == SING_2) rawBottle  = res.value;
  else if (res.channel == SING_0) rawTank    = res.value;
  else if (res.channel == SING_1) {
    rawChamber = res.value;
    completeSetCounter++;

    // --- Pressure conversion ---
    bottlePressure  = voltageToPressure(A.convertToVoltage(rawBottle))  + PRESSURE_OFFSETS[0];
    tankPressure    = voltageToPressure(A.convertToVoltage(rawTank))    + PRESSURE_OFFSETS[1];
    chamberPressure = voltageToPressure(A.convertToVoltage(rawChamber)) + PRESSURE_OFFSETS[2];

    // --- SD log every cycle (full ~96 Hz rate) ---
    logToSD();

    // --- XBee transmit at reduced rate ---
    // Compact JSON ~80 bytes vs ~150 bytes for long-key format.
    // app.py expand_compact() restores full keys before sending to browser.
    if (completeSetCounter % XBEE_OUTPUT_INTERVAL == 0) {
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