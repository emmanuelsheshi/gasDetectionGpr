#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

#define SERVICE_UUID        "e1a2b3c4-d5e6-7890-abcd-ef1234567890"
#define CHARACTERISTIC_UUID "a1b2c3d4-e5f6-7890-abcd-ef1234567890"

#define MQ135_PIN 4
#define MQ137_PIN 3

// --- BLE Client ---
static BLEAdvertisedDevice* metalDetectorDevice = nullptr;
static BLEClient* pClient = nullptr;
static BLERemoteCharacteristic* pRemoteChar = nullptr;
static bool connected = false;
static bool doConnect = false;
static String metalIntensity = "N/A";

// Notification callback — called when the metal detector sends a new intensity value
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify) {
  metalIntensity = String((char*)pData).substring(0, length);
}

// Scan callback — finds the "MetalDetector" device
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == "MetalDetector") {
      Serial.println("Found MetalDetector! Stopping scan...");
      advertisedDevice.getScan()->stop();
      if (metalDetectorDevice) delete metalDetectorDevice;
      metalDetectorDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

void cleanupClient() {
  if (pClient != nullptr) {
    if (pClient->isConnected()) pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }
  pRemoteChar = nullptr;
}

bool connectToServer() {
  Serial.println("Connecting to MetalDetector...");
  cleanupClient();
  pClient = BLEDevice::createClient();

  if (!pClient->connect(metalDetectorDevice)) {
    Serial.println("Failed to connect.");
    cleanupClient();
    return false;
  }
  Serial.println("Connected!");

  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("Failed to find service.");
    pClient->disconnect();
    return false;
  }

  pRemoteChar = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pRemoteChar == nullptr) {
    Serial.println("Failed to find characteristic.");
    pClient->disconnect();
    return false;
  }

  // Subscribe to notifications
  if (pRemoteChar->canNotify()) {
    pRemoteChar->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 20, 21); // RX, TX

  delay(2000); // allow sensors to stabilize a bit

  // --- BLE Client INIT ---
  BLEDevice::init("GasDetector");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false); // Scan for 5 seconds
  Serial.println("BLE scan started, looking for MetalDetector...");
}

void loop() {
  // Check if still connected
  if (connected && pClient != nullptr && !pClient->isConnected()) {
    Serial.println("Disconnected from MetalDetector.");
    cleanupClient();
    connected = false;
    doConnect = false;
    metalIntensity = "N/A";
  }

  // Try to connect if we found the device but haven't connected yet
  if (doConnect && !connected) {
    if (connectToServer()) {
      Serial.println("Connected to MetalDetector BLE server.");
    } else {
      Serial.println("Connection failed. Will rescan...");
      doConnect = false;
    }
  }

  // If disconnected, rescan
  if (!connected && !doConnect) {
    Serial.println("Scanning for MetalDetector...");
    BLEScan* pScan = BLEDevice::getScan();
    pScan->clearResults();
    pScan->start(5, false);
  }

  // Read gas sensors
  int mq135 = analogRead(MQ135_PIN);
  int mq137 = analogRead(MQ137_PIN);

  // Simple filtering
  mq135 = (mq135 + analogRead(MQ135_PIN)) / 2;
  mq137 = (mq137 + analogRead(MQ137_PIN)) / 2;

  // Print all three values: MQ135, MQ137, Metal Intensity
  String data = String(mq135) + "," + String(mq137) + "," + metalIntensity;

  Serial.println(data);   // Debug
  Serial1.println(data);  // Send to Raspberry Pi

  delay(1000);
}