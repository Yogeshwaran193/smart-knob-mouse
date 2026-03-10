/*
 * Phase 5b — BLE Uplink: IMU Data → Laptop
 * 
 * Combines Phase 5a (receive mode commands) with Phase 5b (send IMU data).
 * 
 * Downlink (5a): Laptop sends mode byte → ESP32 changes LED color
 * Uplink  (5b): ESP32 reads BMI160 → sends accel/gyro to laptop via BLE notify
 * 
 * BLE Data Format (6 bytes, sent as notify):
 *   Byte 0-1: accX  (int16, little-endian)
 *   Byte 2-3: accY  (int16, little-endian)
 *   Byte 4-5: accZ  (int16, little-endian)
 * 
 * Wiring:
 *   IMU: SDA=37, SCL=38, 3V3, GND (BMI160 at 0x69)
 *   LED: GPIO 48 (NeoPixel)
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

// ── Pins ──
#define IMU_SDA     37
#define IMU_SCL     38
#define LED_PIN     48
#define BMI160_ADDR 0x69

// ── BLE UUIDs (must match Python) ──
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define MODE_CHAR_UUID      "12345678-1234-5678-1234-56789abcdef1"  // Write: laptop → ESP
#define IMU_DATA_CHAR_UUID  "12345678-1234-5678-1234-56789abcdef2"  // Notify: ESP → laptop

// ── Mode IDs ──
#define MODE_SCROLL     0x01
#define MODE_VOLUME     0x02
#define MODE_ZOOM       0x03
#define MODE_FREEWHEEL  0x04
#define MODE_CLICK      0x05

// ── LED ──
Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

struct ModeInfo {
  const char* name;
  uint8_t r, g, b;
};

const ModeInfo modes[] = {
  {"SCROLL",    0,   0,   255},
  {"VOLUME",    0,   255, 0  },
  {"ZOOM",      255, 0,   0  },
  {"FREEWHEEL", 255, 255, 255},
  {"CLICK",     180, 0,   255},
};

// ── State ──
volatile uint8_t currentMode = MODE_SCROLL;
volatile bool modeChanged = false;
bool bleConnected = false;
bool imuReady = false;

int16_t accX, accY, accZ;
int16_t gyroX, gyroY, gyroZ;

// BLE characteristic for sending IMU data
BLECharacteristic* pImuChar = nullptr;

// ── IMU helpers ──
uint8_t imuReadReg(uint8_t reg) {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom(BMI160_ADDR, (uint8_t)1);
  if (Wire1.available()) return Wire1.read();
  return 0xFF;
}

void imuWriteReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

void readIMU() {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(0x0C);
  Wire1.endTransmission(false);
  Wire1.requestFrom(BMI160_ADDR, (uint8_t)12);

  gyroX = Wire1.read() | (Wire1.read() << 8);
  gyroY = Wire1.read() | (Wire1.read() << 8);
  gyroZ = Wire1.read() | (Wire1.read() << 8);
  accX  = Wire1.read() | (Wire1.read() << 8);
  accY  = Wire1.read() | (Wire1.read() << 8);
  accZ  = Wire1.read() | (Wire1.read() << 8);
}

bool setupIMU() {
  Wire1.begin(IMU_SDA, IMU_SCL);
  Wire1.setClock(400000);

  if (imuReadReg(0x00) != 0xD1) {
    Serial.println("[IMU] BMI160 not found!");
    return false;
  }

  imuWriteReg(0x7E, 0xB6);  // Soft reset
  delay(200);
  imuWriteReg(0x7E, 0x11);  // Accel normal
  delay(50);
  imuWriteReg(0x7E, 0x15);  // Gyro normal
  delay(100);
  imuWriteReg(0x40, 0x28);  // Accel 100Hz
  imuWriteReg(0x41, 0x03);  // +/-2g
  imuWriteReg(0x42, 0x28);  // Gyro 100Hz
  imuWriteReg(0x43, 0x03);  // +/-250 deg/s

  Serial.println("[IMU] BMI160 ready at 0x69");
  return true;
}

// ── LED helper ──
void setLed(uint8_t modeId) {
  const ModeInfo& m = modes[constrain(modeId, MODE_SCROLL, MODE_CLICK) - 1];
  pixel.setPixelColor(0, pixel.Color(m.r, m.g, m.b));
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
        Serial.printf("[BLE] Mode -> %s\n", modes[m - 1].name);
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

  // Mode characteristic (downlink: laptop → ESP)
  BLECharacteristic* pModeChar = svc->createCharacteristic(
    MODE_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pModeChar->setCallbacks(new ModeCB());

  // IMU data characteristic (uplink: ESP → laptop)
  pImuChar = svc->createCharacteristic(
    IMU_DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pImuChar->addDescriptor(new BLE2902());

  svc->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising as 'SmartKnobMouse'");
}

// ── Serial commands (for wired testing) ──
void checkSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  uint8_t m = 0;
  if      (cmd == "S" || cmd == "1") m = MODE_SCROLL;
  else if (cmd == "V" || cmd == "2") m = MODE_VOLUME;
  else if (cmd == "Z" || cmd == "3") m = MODE_ZOOM;
  else if (cmd == "F" || cmd == "4") m = MODE_FREEWHEEL;
  else if (cmd == "C" || cmd == "5") m = MODE_CLICK;
  else return;

  currentMode = m;
  modeChanged = true;
  Serial.printf("[Serial] Mode -> %s\n", modes[m - 1].name);
}

// ── Timing ──
unsigned long lastImuSend = 0;
const unsigned long IMU_INTERVAL = 20;  // 50Hz over BLE (20ms)

// ── Setup ──
void setup() {
  Serial.begin(115200);
  delay(1000);

  pixel.begin();
  pixel.setBrightness(50);

  Serial.println("=== Smart Knob Mouse - Phase 5b ===");
  Serial.println("BLE Uplink: IMU data → Laptop\n");

  imuReady = setupIMU();
  setupBLE();
  setLed(currentMode);

  Serial.println("\nReady! Waiting for BLE connection...\n");
}

// ── Loop ──
void loop() {
  checkSerial();

  // Handle mode change
  if (modeChanged) {
    modeChanged = false;
    setLed(currentMode);
  }

  // Read and send IMU data over BLE
  if (imuReady && bleConnected) {
    unsigned long now = millis();
    if (now - lastImuSend >= IMU_INTERVAL) {
      lastImuSend = now;

      readIMU();

      // Pack 6 bytes: accX, accY, accZ (little-endian int16)
      uint8_t data[6];
      data[0] = accX & 0xFF;
      data[1] = (accX >> 8) & 0xFF;
      data[2] = accY & 0xFF;
      data[3] = (accY >> 8) & 0xFF;
      data[4] = accZ & 0xFF;
      data[5] = (accZ >> 8) & 0xFF;

      pImuChar->setValue(data, 6);
      pImuChar->notify();
    }
  }

  // Also print to serial for debugging
  static unsigned long lastPrint = 0;
  if (imuReady && millis() - lastPrint >= 200) {
    lastPrint = millis();
    readIMU();
    Serial.printf("ACC: %d\t%d\t%d\tBLE:%s\n",
      accX, accY, accZ, bleConnected ? "connected" : "waiting");
  }
}
