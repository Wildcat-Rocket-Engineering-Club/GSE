//STM32F4 - Data acquisition, valve & launch control
// ADS1256 cycleSingle() multi-channel acquisition
// PCA9685 16-ch PWM driver (Adafruit) over I2C1
// HX711 load cell (Rob Tillaart library)
// SD card logging (SdFat by Greiman) + compact JSON telemetry output

#include <ADS1256.h>                    // Modified ADS1256 library
#include <Wire.h>                       // I2C (for PCA9685)
#include <Adafruit_PWMServoDriver.h>    // Adafruit PWM Servo Driver library
#include <HX711.h>                      // Rob Tillaart HX711 library
#include <SdFat.h>                      // SdFat by Bill Greiman (greiman/SdFat @ ^2.2.0)
#include <SPI.h>

#define ARDUINO_ARCH_STM32

/*
 *  STM32F405 PIN ALLOCATION MAP
 *  =============================================
 *  [USED] SPI1 (ADS1256):    PA4(CS), PA5(SCK), PA6(MISO), PA7(MOSI), PA8(DRDY)
 *  [USED] SPI2 (SD CARD):    PB12(CS), PB13(SCK), PB14(MISO), PB15(MOSI)
 *  [USED] UART1 (SERIAL):    PA9(TX), PA10(RX)
 *  [USED] I2C1 (PCA9685):    PB8(SCL), PB9(SDA)
 *  [USED] HX711:             PC6(DOUT), PC7(SCK)  
 *
 *  PCA9685 WIRING:
 *  =============================================
 *  PCA9685 VCC  → 3.3V
 *  PCA9685 GND  → GND
 *  PCA9685 SCL  → PB8
 *  PCA9685 SDA  → PB9
 *  PCA9685 OE   → GND (always enabled)
 *  PCA9685 V+   → Servo power supply (5–6V, NOT STM32 3.3V)
 *
 *  PCA9685 CHANNEL MAP:
 *  =============================================
 *  CH 0  → GSE Fill      (90°)
 *  CH 1  → GSE Relief    (90°)
 *  CH 2  → RKT Ignition  (90°)
 *  CH 3  → RKT Ox        (180°) ← only 180° servo
 *  CH 4  → RKT Fuel      (90°)
 *  CH 5  → RKT Relief    (90°)
 *  CH 6  → RKT Dump      (90°)
 *  CH 7  → GSE Dump      (does not exist / spare)
 *  CH 8–15 → spare
 *
 *  I2C ADDRESS:
 *  Default 0x40 (all address pins A0–A5 low on PCA9685 board).
 *
 *  XBee FLOW CONTROL (wire if packet loss persists):
 *  - XBee CTS → STM32 PA11 (UART1_CTS)
 *  - XBee RTS → STM32 PA12 (UART1_RTS)
 *  Also set XBee firmware: D6=1 (RTS), D7=1 (CTS)
 */

// ============================================================
// PCA9685 CONFIGURATION
// ============================================================

#define PCA9685_ADDR  0x40
#define PCA9685_FREQ  50     // 50 Hz PWM — standard for servos

#define CH_GSE_FILL     0
#define CH_GSE_RELIEF   1
#define CH_RKT_IGN      2
#define CH_RKT_OX       3    // 180° servo
#define CH_RKT_FUEL     4
#define CH_RKT_RELIEF   5
#define CH_RKT_DUMP     6
#define CH_GSE_DUMP     7    // spare / does not exist

// ============================================================
// PER-SERVO PWM ENDPOINTS (microseconds)
// ============================================================

struct ServoConfig {
  uint8_t  channel;
  uint16_t closedUs;
  uint16_t openUs;
};

// ============================================================
// SERIAL PORT
// ============================================================
//#define USE_SERIAL_USB
#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1   // PA9(TX) / PA10(RX)
#endif

const long SERIAL_BAUD = 230400;

// Servo endpoint calibration
// Open | closed
const ServoConfig SERVOS[] = {
  { CH_GSE_FILL,   2510,  1775 },  //  GSE Fill     (90°)
  { CH_GSE_RELIEF, 2050,  970},  //  GSE Relief   (180° but turns 90) 
  { CH_GSE_DUMP,   1930,  970 },  // GSE Dump     (90°, spare)
  { CH_RKT_OX,     3025,  750 },  // RKT Ox       (180°) (*opens more than needed. Can increase from 475)
  { CH_RKT_FUEL,   2200,  960 },  // RKT Fuel     (90°)
  { CH_RKT_RELIEF, 2100,  944 },  // RKT Relief Vent   (90°)
  { CH_RKT_DUMP,   2000,  875 },  // RKT Dump     (90°)
  { CH_RKT_IGN,    875,  2000 },  // RKT Ignition (defaults OFF)
};
const int NUM_SERVOS = sizeof(SERVOS) / sizeof(SERVOS[0]);

// ============================================================
// HX711 LOAD CELL
// ============================================================

// Move HX711 to PC6/PC7 to avoid any trace overlap with I2C/SPI
#define PIN_HX711_DOUT  PC6 
#define PIN_HX711_SCK   PC7

// Calibration values — measured on your specific load cell + wiring
// To re-calibrate: use scale.tare() then scale.calibrate_scale(known_mass)
#define HX711_OFFSET  10304
#define HX711_SCALE   1891.308715f
#define ROCKET_DRY_WEIGHT 85.F

// How often to read the load cell (every N complete ADC cycles).
// HX711 at default 10 SPS — reading more often than ~1/10th of your
// ADC cycle rate just returns stale data. At ~19 Hz XBee output and
// 10 SPS HX711, reading every 10 cycles (~10 Hz) is appropriate.
#define HX711_READ_INTERVAL  10

HX711 scale;
float loadCelllbs = 0.0f;
bool  hx711Ready = false;

// ============================================================
// SD CARD PINS (SPI2)
// ============================================================

#define PIN_SD_CS       PB12
#define PIN_SD_SCK      PB13
#define PIN_SD_MISO     PB14
#define PIN_SD_MOSI     PB15

// ============================================================
// SPI / I2C INSTANCES
// ============================================================

TwoWire wire1(PB9, PB8);   // SDA, SCL — I2C1
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDR, wire1);

SPIClass spi2(PIN_SD_MOSI, PIN_SD_MISO, PIN_SD_SCK);

ADS1256 A(PA8, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &SPI);

// ============================================================
// SD CARD
// ============================================================

SdFat  sd;
SdFile logFile;

// ============================================================
// CONFIGURATION
// ============================================================

const float MAX_VOLTAGE  = 5.0f;
const float MAX_PRESSURE = 5000.0f;
const float PRESSURE_OFFSETS[3] = {-39.0f, -36.0f, -53.0f};

const int XBEE_OUTPUT_INTERVAL = 5;

// ============================================================
// VALVE STATES
// ============================================================

int gse_fill_state    = 0;
int gse_relief_state  = 0;
int gse_dump_state    = 0;
int rkt_ox_state      = 0;
int rkt_fuel_state    = 0;
int rkt_relief_state  = 0;
int rkt_dump_state    = 0;
int rkt_ign_state     = 0;

// ============================================================
// LAUNCH SEQUENCER STATE
// ============================================================

enum LaunchState {
  LAUNCH_IDLE,           // No launch in progress
  LAUNCH_IGNITION,       // 0ms: ignite
  LAUNCH_OX_PENDING,     // Waiting for +200ms to open oxidizer
  LAUNCH_OX_OPEN,        // +200ms: oxidizer opened
  LAUNCH_FUEL_PENDING,   // Waiting for +150ms more to open fuel
  LAUNCH_FUEL_OPEN,      // +350ms: fuel opened, launch complete
};

LaunchState launchState = LAUNCH_IDLE;
unsigned long launchStartTime = 0;

const unsigned long IGNITION_TIME = 0;      // T+0ms
const unsigned long OX_TIME = 500;          // T+500ms
const unsigned long FUEL_TIME = 750;        // T+750ms (500 + 250)

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
// PCA9685 HELPER
// ============================================================

void servoWriteUs(uint8_t channel, uint16_t us) {
  uint16_t tick = (uint16_t)((us / 20000.0f) * 4096.0f);
  pwm.setPWM(channel, 0, tick);
}

// ============================================================
// VALVE CONTROL
// ============================================================

void setValve(uint8_t channel, int &stateVar, int newState) {
  stateVar = newState;
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (SERVOS[i].channel == channel) {
      servoWriteUs(channel, newState == 1 ? SERVOS[i].openUs : SERVOS[i].closedUs);
      // Increased delay to ensure PCA9685 I2C transaction completes before next command
      delay(15);
      return;
    }
  }
  servoWriteUs(channel, newState == 1 ? 1900 : 975);
  delay(15);
}

// ============================================================
// HX711 — INIT
// ============================================================

void initHX711() {

  scale.begin(PIN_HX711_DOUT, PIN_HX711_SCK, true);

  // Wait up to 1 second for the HX711 to become ready
  unsigned long t = millis();
  while (!scale.is_ready() && millis() - t < 1000);

  if (!scale.is_ready()) {
    SERIAL_PORT.println(F("HX711: not ready — check wiring"));
    hx711Ready = false;
    return;
  }

  scale.set_offset(HX711_OFFSET);
  scale.set_scale(HX711_SCALE);

  hx711Ready = true;
  SERIAL_PORT.print(F("HX711: ready, offset="));
  SERIAL_PORT.print(HX711_OFFSET);
  SERIAL_PORT.print(F(", scale="));
  SERIAL_PORT.println(HX711_SCALE);
}

// ============================================================
// HX711 — NON-BLOCKING READ
// Rob Tillaart's library has is_ready() which checks DOUT low
// without blocking. Call this every loop; only reads when data
// is actually available so the ADC cycle is never stalled.
// ============================================================

void updateLoadCell() {
  if (!hx711Ready) return;
  if (!scale.is_ready()) return;   // No new data yet — return immediately

  // get_units() returns the tared, scaled value in your calibrated units (lbs)
  loadCelllbs = loadCelllbs = -scale.get_units(4) - ROCKET_DRY_WEIGHT;  // or even 10;  // 1 = single reading, no averaging
}

// ============================================================
// SD CARD — INIT
// ============================================================

void initSD() {
  spi2.begin();

  SdSpiConfig cfg(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(4), &spi2);

  if (!sd.begin(cfg)) {
    SERIAL_PORT.print(F("SD: begin() FAILED, errorCode="));
    SERIAL_PORT.print(int(sd.sdErrorCode()));
    SERIAL_PORT.print(F(" errorData="));
    SERIAL_PORT.println(int(sd.sdErrorData()));
    sdReady = false;
    return;
  }


  for (int i = 1; i <= 999; i++) {
    snprintf(logFileName, sizeof(logFileName), "LOG_%03d.CSV", i);
    if (!sd.exists(logFileName)) break;
  }

  if (!logFile.open(logFileName, O_RDWR | O_CREAT | O_TRUNC)) {
    sdReady = false;
    return;
  }

  // DEBUG MARKER
  logFile.println("FILE OPENED");
  logFile.flush();

  // Header includes load cell column
  logFile.println(F("row,bottle_psi,tank_psi,chamber_psi,loadcell_lbs,"
                    "gse_fill,gse_relief,gse_dump,"
                    "rkt_ox,rkt_fuel,rkt_relief,rkt_dump,rkt_ign"));
  logFile.sync();

  sdReady = true;
  SERIAL_PORT.print(F("SD: logging to "));
  SERIAL_PORT.println(logFileName);
}

// ============================================================
// SD CARD — LOG ONE ROW
// ============================================================

void logToSD() {
  if (!sdReady) return;
  logFile.println("LOGGING");
  logFile.flush();
  sdRowCount++;
  logFile.print(sdRowCount);            logFile.print(',');
  logFile.print((int)bottlePressure);   logFile.print(',');
  logFile.print((int)tankPressure);     logFile.print(',');
  logFile.print((int)chamberPressure);  logFile.print(',');
  logFile.print(loadCelllbs, 3);         logFile.print(',');  // 3 decimal places
  logFile.print(gse_fill_state);        logFile.print(',');
  logFile.print(gse_relief_state);      logFile.print(',');
  logFile.print(gse_dump_state);        logFile.print(',');
  logFile.print(rkt_ox_state);          logFile.print(',');
  logFile.print(rkt_fuel_state);        logFile.print(',');
  logFile.print(rkt_relief_state);      logFile.print(',');
  logFile.print(rkt_dump_state);        logFile.print(',');
  logFile.println(rkt_ign_state);

  if (sdRowCount % 10 == 0) {
      logFile.flush();
  }
}

// ============================================================
// LAUNCH SEQUENCER
// ============================================================

void startLaunch() {
  launchState = LAUNCH_IGNITION;
  launchStartTime = millis();
  SERIAL_PORT.println(F("LAUNCH SEQUENCE INITIATED"));
}

void updateLaunchSequence() {
  if (launchState == LAUNCH_IDLE) return;

  unsigned long elapsed = millis() - launchStartTime;

  // T+0ms: Ignition
  if (launchState == LAUNCH_IGNITION && elapsed >= IGNITION_TIME) {
    setValve(CH_RKT_IGN, rkt_ign_state, 1);
    SERIAL_PORT.println(F("[LAUNCH] T+0ms: IGNITION"));
    launchState = LAUNCH_OX_PENDING;
  }

  // T+200ms: Open main oxidizer
  if (launchState == LAUNCH_OX_PENDING && elapsed >= OX_TIME) {
    setValve(CH_RKT_OX, rkt_ox_state, 1);
    SERIAL_PORT.println(F("[LAUNCH] T+200ms: MAIN OX OPEN"));
    launchState = LAUNCH_FUEL_PENDING;
  }

  // T+350ms: Open main fuel
  if (launchState == LAUNCH_FUEL_PENDING && elapsed >= FUEL_TIME) {
    setValve(CH_RKT_FUEL, rkt_fuel_state, 1);
    SERIAL_PORT.println(F("[LAUNCH] T+350ms: MAIN FUEL OPEN"));
    launchState = LAUNCH_FUEL_OPEN;
  }
}

// ============================================================
// BULK VALVE CONTROL (OPEN ALL / CLOSE ALL)
// ============================================================

void openAllValves() {
  SERIAL_PORT.println(F("Opening all valves..."));
  setValve(CH_GSE_FILL,   gse_fill_state,   1);
  delay(5);
  setValve(CH_GSE_RELIEF, gse_relief_state, 1);
  delay(5);
  setValve(CH_GSE_DUMP,   gse_dump_state,   1);
  delay(5);
  setValve(CH_RKT_OX,     rkt_ox_state,     1);
  delay(5);
  setValve(CH_RKT_FUEL,   rkt_fuel_state,   1);
  delay(5);
  setValve(CH_RKT_RELIEF, rkt_relief_state, 1);
  delay(5);
  setValve(CH_RKT_DUMP,   rkt_dump_state,   1);
  delay(5);
  setValve(CH_RKT_IGN,    rkt_ign_state,    1);
  SERIAL_PORT.println(F("All valves opened"));
}

void closeAllValves() {
  SERIAL_PORT.println(F("Closing all valves..."));
  setValve(CH_GSE_FILL,   gse_fill_state,   0);
  delay(5);
  setValve(CH_GSE_RELIEF, gse_relief_state, 0);
  delay(5);
  setValve(CH_GSE_DUMP,   gse_dump_state,   0);
  delay(5);
  setValve(CH_RKT_OX,     rkt_ox_state,     0);
  delay(5);
  setValve(CH_RKT_FUEL,   rkt_fuel_state,   0);
  delay(5);
  setValve(CH_RKT_RELIEF, rkt_relief_state, 0);
  delay(5);
  setValve(CH_RKT_DUMP,   rkt_dump_state,   0);
  delay(5);
  setValve(CH_RKT_IGN,    rkt_ign_state,    0);
  SERIAL_PORT.println(F("All valves closed"));
}

// ============================================================
// SERIAL COMMAND PARSER
// ============================================================

void handleSerialCommand(String &line) {
  // Check for launch command first
  if (line.indexOf("\"launch\"") != -1 || line.indexOf("launch") != -1) {
    startLaunch();
    return;
  }

  // Check for bulk valve commands (compact form: oa=open_all, ca=close_all)
  if (line.indexOf("\"oa\"") != -1 || line.indexOf("oa") != -1) {
    openAllValves();
    return;
  }

  if (line.indexOf("\"ca\"") != -1 || line.indexOf("ca") != -1) {
    closeAllValves();
    return;
  }

  if (line.indexOf("set_valve") == -1) return;

  int state    = 0;
  int stateIdx = line.indexOf("\"state\":");
  if (stateIdx != -1) {
    int valIdx = stateIdx + 8;
    while (valIdx < (int)line.length() && line[valIdx] == ' ') valIdx++;
    state = (line[valIdx] == '1') ? 1 : 0;
  }

  if      (line.indexOf("\"gf\"") != -1 || line.indexOf("gse_fill")      != -1) setValve(CH_GSE_FILL,   gse_fill_state,   state);
  else if (line.indexOf("\"gr\"") != -1 || line.indexOf("gse_relief")    != -1) setValve(CH_GSE_RELIEF, gse_relief_state, state);
  else if (line.indexOf("\"gd\"") != -1 || line.indexOf("gse_dump")      != -1) setValve(CH_GSE_DUMP,   gse_dump_state,   state);
  else if (line.indexOf("\"ro\"") != -1 || line.indexOf("rocket_ox")     != -1) setValve(CH_RKT_OX,     rkt_ox_state,     state);
  else if (line.indexOf("\"rf\"") != -1 || line.indexOf("rocket_fuel")   != -1) setValve(CH_RKT_FUEL,   rkt_fuel_state,   state);
  else if (line.indexOf("\"rr\"") != -1 || line.indexOf("rocket_relief") != -1) setValve(CH_RKT_RELIEF, rkt_relief_state, state);
  else if (line.indexOf("\"rd\"") != -1 || line.indexOf("rocket_dump")   != -1) setValve(CH_RKT_DUMP,   rkt_dump_state,   state);
  else if (line.indexOf("\"ig\"") != -1 || line.indexOf("ignite")        != -1) setValve(CH_RKT_IGN,    rkt_ign_state,    state);
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

  // --- Serial ---
  SERIAL_PORT.begin(SERIAL_BAUD);
  while (!SERIAL_PORT && millis() < 5000);
  SERIAL_PORT.println(F("Booting..."));

  // --- PCA9685 ---
  wire1.begin();
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(PCA9685_FREQ);
  delay(10);
  for (int i = 0; i < NUM_SERVOS; i++) {
    servoWriteUs(SERVOS[i].channel, SERVOS[i].closedUs);
  }
  SERIAL_PORT.println(F("PCA9685: all valves closed"));

  // --- HX711 ---
  initHX711();

  // --- ADS1256 (SPI1) ---
  A.InitializeADC();
  A.setPGA(PGA_1);
  A.setBuffer(0);
  A.setDRATE(DRATE_2000SPS);
  delay(100);
  A.sendDirectCommand(SELFCAL);
  delay(200);
  SERIAL_PORT.println(F("ADS1256: initialized"));

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
  while (SERIAL_PORT.available()) {
    char c = SERIAL_PORT.read();
    if (c == '\n') {
      handleSerialCommand(serialBuffer);
      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }

  // --- Non-blocking launch sequencer ---
  // Runs every iteration without blocking telemetry
  updateLaunchSequence();

  // --- HX711 non-blocking read ---
  // is_ready() returns true only when new data is available (DOUT goes low).
  // This never blocks — if no data is ready it returns immediately.
  updateLoadCell();

  // --- ADC cycling ---
  const uint8_t channels[3] = {SING_0, SING_1, SING_2};
  ADS1256::ADS1256_Result res = A.cycleSingle3Tracked(channels, 3);

  if (res.channel == 0xFF) return;

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

    // --- SD log every cycle (full ~96 Hz) ---
    logToSD();

    // --- XBee transmit at reduced rate ---
    // Load cell value is included in telemetry — it updates at ~10 Hz
    // independently and the last read value is always transmitted.
    if (completeSetCounter % XBEE_OUTPUT_INTERVAL == 0) {
      SERIAL_PORT.print(F("{\"p\":{\"b\":"));
      SERIAL_PORT.print((int)bottlePressure);
      SERIAL_PORT.print(F(",\"t\":"));
      SERIAL_PORT.print((int)tankPressure);
      SERIAL_PORT.print(F(",\"c\":"));
      SERIAL_PORT.print((int)chamberPressure);
      SERIAL_PORT.print(F("},\"g\":{\"l\":"));
      SERIAL_PORT.print(loadCelllbs, 2);   // 2 decimal places in telemetry
      SERIAL_PORT.print(F(",\"f\":"));
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