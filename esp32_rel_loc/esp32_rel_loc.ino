#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define NODE_ID 0  // 0, 1 o 2 a seconda della scheda

struct NodeState {
  float x = 0.0;
  float y = 0.0;
};

NodeState nodeState;

void handleRSSIReading(uint8_t nodeId, int rssi) {
  Serial.printf("[Node %d] RSSI: %d\n", nodeId, rssi);
}

class ScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String data = advertisedDevice.getManufacturerData();
      if (data.length() >= 3 &&
          (uint8_t)data[0] == 0xFF &&
          (uint8_t)data[1] == 0xFF) {
        uint8_t nodeId = data[2];
        int rssi = advertisedDevice.getRSSI();
        handleRSSIReading(nodeId, rssi);
      }
    }
  }
};

void setupBroadcaster() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData advData;
  uint8_t payload[11];
  payload[0] = 0xFF; payload[1] = 0xFF;    // Company ID (test)
  payload[2] = NODE_ID;                    // Node ID
  memcpy(&payload[3], &nodeState.x, 4);    // Coordinata x
  memcpy(&payload[7], &nodeState.y, 4);    // Coordinata y
  String payloadStr((char*)payload, 11);
  advData.setManufacturerData(payloadStr);
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();
}

void setupObserver() {
  BLEScan *pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new ScanCallback());
  pScan->setActiveScan(true);
  pScan->start(0, nullptr, false);
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32_NODE");
  setupBroadcaster();
  setupObserver();
}

void loop() {
  delay(3000);
  BLEScan* pScan = BLEDevice::getScan();
  pScan->stop();
  pScan->clearResults();
  pScan->start(0, nullptr, false);
}