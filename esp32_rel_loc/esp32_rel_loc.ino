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
  float distances[3] = {0.0, 0.0, 0.0};
  float externalDistance = -1.0;
  float posX[3] = {0.0, 0.0, 0.0};
  float posY[3] = {0.0, 0.0, 0.0};
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
    /* Serial.printf("[Node %d] Raw RSSI: %d | Filtered RSSI: %d -> Dist: %.2f\n",
      nodeId, rssi, filteredRSSI, nodeState.distances[nodeId]); */
  }
}

void handlePeerDistances(uint8_t senderId, float senderDistances[3]) {
  for (uint8_t i = 0; i < 3; i++) {
    if (i != NODE_ID && i != senderId) {
      nodeState.externalDistance = senderDistances[i];
      /* Serial.printf("[Peer %d] Extracted external distance d(%d): %.2f\n",
        senderId, i, senderDistances[i]); */
    }
  }
}

void computePositions() {
  if (nodeState.externalDistance < 0) return;

  float d01, d02, d12;

  if (NODE_ID == 0) {
    d01 = nodeState.distances[1];
    d02 = nodeState.distances[2];
    d12 = nodeState.externalDistance;
  } else if (NODE_ID == 1) {
    d01 = nodeState.distances[0];
    d12 = nodeState.distances[2];
    d02 = nodeState.externalDistance;
  } else {
    d02 = nodeState.distances[0];
    d12 = nodeState.distances[1];
    d01 = nodeState.externalDistance;
  }

  nodeState.posX[0] = 0.0;
  nodeState.posY[0] = 0.0;
  nodeState.posX[1] = d01;
  nodeState.posY[1] = 0.0;

  float x2 = (d02 * d02 - d12 * d12 + d01 * d01) / (2 * d01);
  float y2Squared = d02 * d02 - x2 * x2;
  nodeState.posX[2] = x2;
  nodeState.posY[2] = (y2Squared > 0) ? sqrt(y2Squared) : 0.0;

  Serial.printf("[Positions] N0: (0.0, 0.0) | N1: (%.2f, 0.0) | N2: (%.2f, %.2f)\n",
    nodeState.posX[1], nodeState.posX[2], nodeState.posY[2]);
}

class ScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String data = advertisedDevice.getManufacturerData();
      if (data.length() >= 15 &&
          (uint8_t)data[0] == 0xFF &&
          (uint8_t)data[1] == 0xFF) {
        uint8_t senderId = data[2];
        int rssi = advertisedDevice.getRSSI();
        handleRSSIReading(senderId, rssi);

        float senderDistances[3];
        memcpy(senderDistances, &data[3], 12);
        handlePeerDistances(senderId, senderDistances);
      }
    }
  }
};

void updateAdvertisingPayload() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData advData;
  uint8_t payload[15];
  payload[0] = 0xFF; payload[1] = 0xFF;  // Company ID
  payload[2] = NODE_ID;                  // Node ID
  memcpy(&payload[3], &nodeState.distances[0], 4);
  memcpy(&payload[7], &nodeState.distances[1], 4);
  memcpy(&payload[11], &nodeState.distances[2], 4);
  String payloadStr((char*)payload, 15);
  advData.setManufacturerData(payloadStr);
  pAdvertising->setAdvertisementData(advData);
}

void setupBroadcaster() {
  updateAdvertisingPayload();
  BLEDevice::getAdvertising()->start();
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

  computePositions();
  updateAdvertisingPayload();
}