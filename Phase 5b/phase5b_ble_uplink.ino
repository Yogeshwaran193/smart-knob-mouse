/*
 * Phase 5b — BLE Uplink: IMU Only
 * 
 * Reads BMI160 and sends accel data to laptop over BLE notify.
 * No mode switching, no LED patterns. Just IMU → BLE.
 * 
 * BLE Data Format (6 bytes):
 *   Byte 0-1: accX  (int16, little-endian)
 *   Byte 2-3: accY  (int16, little-endian)
 *   Byte 4-5: accZ  (int16, little-endian)
 * 
 * Wiring:
 *   IMU: SDA=37, SCL=38, 3V3, GND (BMI160 at 0x69)
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define IMU_SDA     37
#define IMU_SCL     38
#define BMI160_ADDR 0x69

#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define IMU_DATA_CHAR_UUID  "12345678-1234-5678-1234-56789abcdef2"

int16_t accX, accY, accZ;
bool bleConnected = false;

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

  // Skip gyro (6 bytes)
  Wire1.read(); Wire1.read();
  Wire1.read(); Wire1.read();
  Wire1.read(); Wire1.read();
  // Read accel
  accX = Wire1.read() | (Wire1.read() << 8);
  accY = Wire1.read() | (Wire1.read() << 8);
  accZ = Wire1.read() | (Wire1.read() << 8);
}

bool setupIMU() {
  Wire1.begin(IMU_SDA, IMU_SCL);
  Wire1.setClock(400000);

  if (imuReadReg(0x00) != 0xD1) {
    Serial.println("[IMU] BMI160 not found!");
    return false;
  }

  imuWriteReg(0x7E, 0xB6);  delay(200);  // Reset
  imuWriteReg(0x7E, 0x11);  delay(50);   // Accel normal
  imuWriteReg(0x7E, 0x15);  delay(100);  // Gyro normal
  imuWriteReg(0x40, 0x28);  // Accel 100Hz
  imuWriteReg(0x41, 0x03);  // +/-2g

  Serial.println("[IMU] BMI160 ready");
  return true;
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

void setupBLE() {
  BLEDevice::init("SmartKnobMouse");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  BLEService* svc = server->createService(SERVICE_UUID);

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

// ── Timing ──
unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Phase 5b: BLE IMU Uplink ===\n");

  if (!setupIMU()) {
    while (1) delay(1000);
  }

  setupBLE();
  Serial.println("\nWaiting for BLE connection...\n");
}

void loop() {
  if (bleConnected && millis() - lastSend >= 20) {  // 50Hz
    lastSend = millis();

    readIMU();

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

  // Debug print at 5Hz
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 200) {
    lastPrint = millis();
    readIMU();
    Serial.printf("ACC: %d\t%d\t%d\tBLE:%s\n",
      accX, accY, accZ, bleConnected ? "yes" : "no");
  }
}
