/*
 * Phase 5a — BLE Downlink: Mode → RGB LED Pattern
 * 
 * ESP32-S3 N16R8 — Onboard LED is WS2812 RGB on GPIO 48
 * Uses Adafruit NeoPixel library (install via Arduino Library Manager)
 * 
 * Each mode = unique COLOR + BLINK PATTERN:
 *   SCROLL    (0x01) — BLUE,   slow blink
 *   VOLUME    (0x02) — GREEN,  two quick blinks, pause
 *   ZOOM      (0x03) — RED,    fast blink
 *   FREEWHEEL (0x04) — WHITE,  always on (steady)
 *   CLICK     (0x05) — PURPLE, single flash, long pause
 * 
 * Author: Yogeshwaran
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

// ── RGB LED Setup ──
#define LED_PIN    48
#define LED_COUNT  1
#define BRIGHTNESS 50    // 0-255, keep low so it doesn't blind you

Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ── BLE UUIDs (must match Python scripts) ──
#define SERVICE_UUID    "12345678-1234-5678-1234-56789abcdef0"
#define MODE_CHAR_UUID  "12345678-1234-5678-1234-56789abcdef1"

// ── Mode IDs ──
#define MODE_SCROLL     0x01
#define MODE_VOLUME     0x02
#define MODE_ZOOM       0x03
#define MODE_FREEWHEEL  0x04
#define MODE_CLICK      0x05

// ── Color for each mode (R, G, B) ──
struct ModeInfo {
  const char* name;
  uint8_t r, g, b;
};

const ModeInfo modes[] = {
  {"SCROLL",    0,   0,   255},  // Blue
  {"VOLUME",    0,   255, 0  },  // Green
  {"ZOOM",      255, 0,   0  },  // Red
  {"FREEWHEEL", 255, 255, 255},  // White
  {"CLICK",     180, 0,   255},  // Purple
};

// ── State ──
volatile uint8_t currentMode = MODE_SCROLL;
volatile bool modeChanged = false;
bool bleConnected = false;

// LED pattern state
unsigned long prevMillis = 0;
int step = 0;
bool ledOn = false;

// ── Helper: get mode info (index = modeId - 1) ──
const ModeInfo& getMode(uint8_t id) {
  return modes[constrain(id, MODE_SCROLL, MODE_CLICK) - 1];
}

// ── Helper: set LED color or off ──
void setLed(bool on) {
  ledOn = on;
  if (on) {
    const ModeInfo& m = getMode(currentMode);
    pixel.setPixelColor(0, pixel.Color(m.r, m.g, m.b));
  } else {
    pixel.setPixelColor(0, 0);  // Off
  }
  pixel.show();
}

// ── BLE Callbacks ──
class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer* s) {
    bleConnected = true;
    Serial.println("[BLE] Connected");
  }
  void onDisconnect(BLEServer* s) {
    bleConnected = false;
    Serial.println("[BLE] Disconnected");
    BLEDevice::startAdvertising();
  }
};

class ModeCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {
    String val = c->getValue();
    if (val.length() >= 1) {
      uint8_t m = (uint8_t)val[0];
      if (m >= MODE_SCROLL && m <= MODE_CLICK) {
        currentMode = m;
        modeChanged = true;
        Serial.printf("[BLE] Mode -> %s (0x%02X)\n", getMode(m).name, m);
      }
    }
  }
};

// ── BLE Setup ──
void setupBLE() {
  BLEDevice::init("SmartKnobMouse");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  BLEService* svc = server->createService(SERVICE_UUID);

  BLECharacteristic* modeChar = svc->createCharacteristic(
    MODE_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  modeChar->setCallbacks(new ModeCB());

  svc->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising as 'SmartKnobMouse'");
}

// ── Serial Command Check ──
void checkSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  uint8_t m = 0;
  if      (cmd == "S" || cmd == "SCROLL"    || cmd == "1") m = MODE_SCROLL;
  else if (cmd == "V" || cmd == "VOLUME"    || cmd == "2") m = MODE_VOLUME;
  else if (cmd == "Z" || cmd == "ZOOM"      || cmd == "3") m = MODE_ZOOM;
  else if (cmd == "F" || cmd == "FREEWHEEL" || cmd == "4") m = MODE_FREEWHEEL;
  else if (cmd == "C" || cmd == "CLICK"     || cmd == "5") m = MODE_CLICK;
  else {
    Serial.println("Commands: S=Scroll  V=Volume  Z=Zoom  F=Freewheel  C=Click");
    return;
  }

  currentMode = m;
  modeChanged = true;
  Serial.printf("[Serial] Mode -> %s (0x%02X)\n", getMode(m).name, m);
}

// ── LED Blink Patterns (non-blocking) ──
void runLedPattern() {
  unsigned long now = millis();

  switch (currentMode) {

    case MODE_SCROLL: {
      // BLUE slow blink: 500ms on, 500ms off
      if (now - prevMillis >= 500) {
        prevMillis = now;
        setLed(!ledOn);
      }
      break;
    }

    case MODE_VOLUME: {
      // GREEN two quick blinks then pause
      // ON 120 -> OFF 120 -> ON 120 -> OFF 600
      const unsigned long t[] = {120, 120, 120, 600};
      if (now - prevMillis >= t[step % 4]) {
        prevMillis = now;
        setLed(step % 2 == 0);
        step = (step + 1) % 4;
      }
      break;
    }

    case MODE_ZOOM: {
      // RED fast blink: 100ms on, 100ms off
      if (now - prevMillis >= 100) {
        prevMillis = now;
        setLed(!ledOn);
      }
      break;
    }

    case MODE_FREEWHEEL: {
      // WHITE always on
      if (!ledOn) {
        setLed(true);
      }
      break;
    }

    case MODE_CLICK: {
      // PURPLE short flash, long dark
      // ON 80ms -> OFF 1200ms
      const unsigned long t[] = {80, 1200};
      if (now - prevMillis >= t[step % 2]) {
        prevMillis = now;
        setLed(step % 2 == 0);
        step = (step + 1) % 2;
      }
      break;
    }
  }
}

// ── Setup ──
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init NeoPixel
  pixel.begin();
  pixel.setBrightness(BRIGHTNESS);
  pixel.show();  // Start with LED off

  Serial.println("=== Smart Knob Mouse - Phase 5a ===");
  Serial.println("Mode -> RGB LED pattern test");
  Serial.println("Send: S / V / Z / F / C");
  Serial.println();

  setupBLE();

  Serial.printf("Default: %s\n\n", getMode(currentMode).name);
}

// ── Loop ──
void loop() {
  checkSerial();

  if (modeChanged) {
    modeChanged = false;
    step = 0;
    prevMillis = millis();
    setLed(false);  // Brief off before new pattern starts
    Serial.printf("LED -> %s (%s)\n",
      getMode(currentMode).name,
      currentMode == MODE_SCROLL    ? "BLUE" :
      currentMode == MODE_VOLUME    ? "GREEN" :
      currentMode == MODE_ZOOM      ? "RED" :
      currentMode == MODE_FREEWHEEL ? "WHITE" : "PURPLE");
  }

  runLedPattern();
}
