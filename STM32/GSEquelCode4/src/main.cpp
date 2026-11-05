//STM32F4 - Data acquisition, valve & launch control
// ADS1256 cycleSingle() multi-channel acquisition
// PCA9685 16-ch PWM driver (Adafruit) over I2C1
// HX711 load cell (Rob Tillaart library)
// SD card logging (SdFat by Greiman) + compact JSON telemetry output

#include "ADS1256PressureSensor.h"
#include "HX711LoadCell.h"
#include "SDLogger.h"
#include "SerialProtocol.h"
#include "ValveController.h"

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
// SERIAL PORT
// ============================================================
//#define USE_SERIAL_USB
#ifdef USE_SERIAL_USB
  #define SERIAL_PORT Serial
#else
  #define SERIAL_PORT Serial1   // PA9(TX) / PA10(RX)
#endif

const long SERIAL_BAUD = 230400;

ADS1256PressureSensor pressureSensor;
HX711LoadCell loadCell;
SDLogger sdLogger;
ValveController valves;
SerialProtocol serialProtocol;

// ============================================================
// CONFIGURATION
// ============================================================

const int XBEE_OUTPUT_INTERVAL = 5;

// ============================================================
// ADC / PRESSURE GLOBALS
// ============================================================

int           completeSetCounter = 0;
PressureReadings pressureReadings = {0.0f, 0.0f, 0.0f};

void logToSD() {
  const ValveStates &valveStates = valves.states();
  const SDLogRecord record = {
      pressureReadings.bottlePsi,
      pressureReadings.tankPsi,
      pressureReadings.chamberPsi,
      loadCell.valueLbs(),
      valveStates.gseFill,
      valveStates.gseRelief,
      valveStates.gseDump,
      valveStates.rocketOx,
      valveStates.rocketFuel,
      valveStates.rocketRelief,
      valveStates.rocketDump,
      valveStates.rocketIgnition};
  sdLogger.logRow(record, SERIAL_PORT);
}

// ============================================================
// SERIAL COMMAND PARSER
// ============================================================

void dispatchSerialCommand(const SerialCommand &command) {
  switch (command.type) {
    case SerialCommandType::Launch:
      valves.startLaunch(SERIAL_PORT);
      break;
    case SerialCommandType::OpenAll:
      valves.openAll(SERIAL_PORT);
      break;
    case SerialCommandType::CloseAll:
      valves.closeAll(SERIAL_PORT);
      break;
    case SerialCommandType::SetValve:
      valves.setValve(command.valve, command.state);
      break;
    case SerialCommandType::None:
      break;
  }
}

void updatePressureAcquisition() {
  if (!pressureSensor.update(pressureReadings)) return;

  const ValveStates &valveStates = valves.states();
  completeSetCounter++;
  logToSD();

  if (completeSetCounter % XBEE_OUTPUT_INTERVAL == 0) {
    const SerialTelemetry telemetry = {
        pressureReadings, loadCell.valueLbs(), valveStates};
    serialProtocol.writeTelemetry(SERIAL_PORT, telemetry);
  }
}

// ============================================================
// SETUP
// ============================================================

void initializeSerialPort() {
  SERIAL_PORT.begin(SERIAL_BAUD);
  while (!SERIAL_PORT && millis() < 5000);
  SERIAL_PORT.println(F("Booting..."));
}

void initializeHardwareModules() {
  // --- PCA9685 / valves ---
  valves.begin(SERIAL_PORT);

  // --- HX711 ---
  loadCell.begin(SERIAL_PORT);

  // --- ADS1256 (SPI1) ---
  if (pressureSensor.begin()) {
    SERIAL_PORT.println(F("ADS1256: initialized"));
  } else {
    SERIAL_PORT.println(F("ADS1256: not detected (DRDY stayed HIGH; check module and wiring)"));
  }

  // --- SD card (SPI2) ---
  sdLogger.begin(SERIAL_PORT);

  SERIAL_PORT.print(F("Ready. SD="));
  SERIAL_PORT.println(sdLogger.ready() ? sdLogger.fileName() : "FAIL");
}

void setup() {
  initializeSerialPort();
  initializeHardwareModules();
}

// ============================================================
// LOOP
// ============================================================

void processSerialInput() {
  while (SERIAL_PORT.available()) {
    SerialCommand command;
    if (serialProtocol.receive(SERIAL_PORT.read(), command)) {
      dispatchSerialCommand(command);
    }
  }
}

void updateControlAndTelemetry() {
  // --- Launch sequencer ---
  valves.updateLaunchSequence(SERIAL_PORT);

  // --- HX711 read ---
  loadCell.update();

  // --- Pressure acquisition and telemetry ---
  updatePressureAcquisition();
}

void loop() {
  processSerialInput();
  updateControlAndTelemetry();
}