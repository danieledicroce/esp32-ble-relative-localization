#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define NODE_ID 0  // 0, 1 o 2 a seconda della scheda

#define RSSI_AT_1M -56.0       // RSSI(d0), da ricalibrare per ogni ambiente
#define PATH_LOSS_EXPONENT 3.0 // n, da ricalibrare per ogni ambiente

struct NodeState {
  float x = 0.0;
  float y = 0.0;
  float distances[3] = {0.0, 0.0, 0.0};  // indicizzato per Node ID
};

NodeState nodeState;

float distanceFromRSSI(int rssi) {
  return pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
}

void handleRSSIReading(uint8_t nodeId, int rssi) {
  if (nodeId < 3) {
    nodeState.distances[nodeId] = distanceFromRSSI(rssi);
    Serial.printf("[Node %d] RSSI: %d -> Dist: %.2f\n", nodeId, rssi, nodeState.distances[nodeId]);
  }
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
  payload[0] = 0xFF; payload[1] = 0xFF;    // Company ID
  payload[2] = NODE_ID;                    // Node ID
  memcpy(&payload[3], &nodeState.x, 4);
  memcpy(&payload[7], &nodeState.y, 4);
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