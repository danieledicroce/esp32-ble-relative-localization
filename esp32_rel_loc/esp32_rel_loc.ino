#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define NODE_ID 0  // 0, 1 o 2 a seconda della scheda

#define RSSI_AT_1M -56.0        // RSSI(d0), da ricalibrare per ogni ambiente
#define PATH_LOSS_EXPONENT 3.0  // n, idem
#define PROCESS_NOISE 0.5       // Q, idem
#define MEASUREMENT_NOISE 10.0  // R, idem

struct KalmanState {
  float estimate = 0.0;
  float errorCovariance = 1.0;
  bool initialized = false;
};

struct NodeState {
  float x = 0.0;
  float y = 0.0;
  float distances[3] = {0.0, 0.0, 0.0};
  KalmanState kalman[3];
};

NodeState nodeState;


float distanceFromRSSI(int rssi) {
  return pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
}

int kalmanFilterRSSI(uint8_t nodeId, int measuredRSSI) {
  if (nodeId >= 3) return measuredRSSI;
  KalmanState &state = nodeState.kalman[nodeId];
  if (!state.initialized) {
    state.estimate = measuredRSSI;
    state.initialized = true;
    return measuredRSSI;
  }
  float predictedEstimate = state.estimate;
  float predictedCovariance = state.errorCovariance + PROCESS_NOISE;
  float kalmanGain = predictedCovariance / (predictedCovariance + MEASUREMENT_NOISE);
  state.estimate = predictedEstimate + kalmanGain * (measuredRSSI - predictedEstimate);
  state.errorCovariance = (1.0 - kalmanGain) * predictedCovariance;
  return (int)round(state.estimate);
}

void handleRSSIReading(uint8_t nodeId, int rssi) {
  if (nodeId < 3) {
    int filteredRSSI = kalmanFilterRSSI(nodeId, rssi);
    nodeState.distances[nodeId] = distanceFromRSSI(filteredRSSI);
    Serial.printf("[Node %d] Raw RSSI: %d | Filtered RSSI: %d -> Dist: %.2f\n",
      nodeId, rssi, filteredRSSI, nodeState.distances[nodeId]);
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