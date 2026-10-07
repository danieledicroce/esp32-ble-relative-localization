#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define NODE_ID 0      // 0, 1 o 2 a seconda della scheda
#define EXTERNAL_ID 3  // identificativo del dispositivo esterno da localizzare

#define RSSI_AT_1M -56.0        // RSSI(d0), da ricalibrare per ogni ambiente
#define PATH_LOSS_EXPONENT 3.0  // n, idem
#define PROCESS_NOISE 0.5       // Q, idem
#define MEASUREMENT_NOISE 10.0  // R, idem

#define CALIBRATION_MODE false

struct KalmanState {
  float estimate = 0.0;
  float errorCovariance = 1.0;
  bool initialized = false;
};

struct FixedNodesState {
  float distances[3] = {0.0, 0.0, 0.0};    // proprie distanze misurate verso i nodi fissi
  float externalDistance = -1.0;           // distanza tra gli altri due nodi
  float posX[3] = {0.0, 0.0, 0.0};         // posizioni calcolate dei nodi fissi
  float posY[3] = {0.0, 0.0, 0.0};
  KalmanState kalman[3];                   // indicizzato per Node ID
};

struct ExternalDeviceState {
  float distance = -1.0;                        // propria distanza misurata dal dispositivo esterno
  float peerDistances[3] = {-1.0, -1.0, -1.0};  // distanza dal dispositivo esterno per ciascun nodo, propria inclusa
  float x = 0.0;
  float y = 0.0;
  KalmanState kalman;
};

struct NodeState {
  FixedNodesState fixedNodes;
  ExternalDeviceState device;
};

NodeState nodeState;

float distanceFromRSSI(int rssi) {
  return pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
}

int kalmanFilterRSSI(KalmanState &state, int measuredRSSI) {
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
    int filteredRSSI = kalmanFilterRSSI(nodeState.fixedNodes.kalman[nodeId], rssi);
    nodeState.fixedNodes.distances[nodeId] = distanceFromRSSI(filteredRSSI);

    if (CALIBRATION_MODE) {
      Serial.printf("[Node %d] Raw RSSI: %d | Filtered RSSI: %d -> Dist: %.2f\n",
      nodeId, rssi, filteredRSSI, nodeState.fixedNodes.distances[nodeId]);
    }

  } else if (nodeId == EXTERNAL_ID) {
    int filteredRSSI = kalmanFilterRSSI(nodeState.device.kalman, rssi);
    nodeState.device.distance = distanceFromRSSI(filteredRSSI);
    nodeState.device.peerDistances[NODE_ID] = nodeState.device.distance;
    /* Serial.printf("[Target Device] Raw RSSI: %d | Filtered RSSI: %d -> Dist: %.2f\n",
      rssi, filteredRSSI, nodeState.device.distance); */
  }
}

void handlePeerData(uint8_t senderId, float peerDistances[3], float peerDeviceDistance) {
  for (uint8_t i = 0; i < 3; i++) {
    if (i != NODE_ID && i != senderId) {
      nodeState.fixedNodes.externalDistance = peerDistances[i];
    }
  }
  nodeState.device.peerDistances[senderId] = peerDeviceDistance;
  /* Serial.printf("[Peer %d] ExtDist: %.2f | Peer->Target Dist: %.2f\n",
    senderId, nodeState.fixedNodes.externalDistance, peerDeviceDistance); */
}

void trilaterateFromBaseline(float d0, float d1, float d01, float &x, float &y) {
  x = (d0 * d0 - d1 * d1 + d01 * d01) / (2 * d01);
  float ySquared = d0 * d0 - x * x;
  y = (ySquared > 0) ? sqrt(ySquared) : 0.0;
}

void computePositions() {
  if (nodeState.fixedNodes.externalDistance < 0) return;

  float d01, d02, d12;

  if (NODE_ID == 0) {
    d01 = nodeState.fixedNodes.distances[1];
    d02 = nodeState.fixedNodes.distances[2];
    d12 = nodeState.fixedNodes.externalDistance;
  } else if (NODE_ID == 1) {
    d01 = nodeState.fixedNodes.distances[0];
    d12 = nodeState.fixedNodes.distances[2];
    d02 = nodeState.fixedNodes.externalDistance;
  } else {
    d02 = nodeState.fixedNodes.distances[0];
    d12 = nodeState.fixedNodes.distances[1];
    d01 = nodeState.fixedNodes.externalDistance;
  }

  nodeState.fixedNodes.posX[0] = 0.0;
  nodeState.fixedNodes.posY[0] = 0.0;
  nodeState.fixedNodes.posX[1] = d01;
  nodeState.fixedNodes.posY[1] = 0.0;

  trilaterateFromBaseline(d02, d12, d01, nodeState.fixedNodes.posX[2], nodeState.fixedNodes.posY[2]);
  
  /* Serial.printf("[Positions] N0: (0.0, 0.0) | N1: (%.2f, 0.0) | N2: (%.2f, %.2f)\n",
    nodeState.fixedNodes.posX[1], nodeState.fixedNodes.posX[2], nodeState.fixedNodes.posY[2]); */
}

void computeDevicePosition() {
  if (nodeState.device.peerDistances[0] < 0 ||
      nodeState.device.peerDistances[1] < 0 ||
      nodeState.device.peerDistances[2] < 0) return;

  float d0 = nodeState.device.peerDistances[0];
  float d1 = nodeState.device.peerDistances[1];
  float d2 = nodeState.device.peerDistances[2];

  float x, yCandidate;
  trilaterateFromBaseline(d0, d1, nodeState.fixedNodes.posX[1], x, yCandidate);

  float x2 = nodeState.fixedNodes.posX[2];
  float y2 = nodeState.fixedNodes.posY[2];

  float distIfPositive = sqrt(pow(x - x2, 2) + pow(yCandidate - y2, 2));
  float distIfNegative = sqrt(pow(x - x2, 2) + pow(-yCandidate - y2, 2));

  nodeState.device.x = x;
  nodeState.device.y = (fabs(distIfPositive - d2) <= fabs(distIfNegative - d2)) ? yCandidate : -yCandidate;

  /* Serial.printf("[Target Position] X: %.2f | Y: %.2f (Chosen Y Candidate: %.2f)\n",
    nodeState.device.x, nodeState.device.y, yCandidate); */
}

class ScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String data = advertisedDevice.getManufacturerData();

      if (data.length() >= 3 &&
          (uint8_t)data[0] == 0xFF &&
          (uint8_t)data[1] == 0xFF) {

        uint8_t senderId = data[2];
        int rssi = advertisedDevice.getRSSI();
        handleRSSIReading(senderId, rssi);

        if (senderId < 3 && data.length() >= 19) {
          float senderDistances[3];
          memcpy(senderDistances, &data[3], 12);

          float senderDeviceDistance;
          memcpy(&senderDeviceDistance, &data[15], 4);

          handlePeerData(senderId, senderDistances, senderDeviceDistance);
        }
      }
    }
  }
};

void updateAdvertisingPayload() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advData;
  uint8_t payload[19];
  payload[0] = 0xFF; payload[1] = 0xFF;    // Company ID (test/prototipazione)
  payload[2] = NODE_ID;                    // Node ID
  memcpy(&payload[3], nodeState.fixedNodes.distances, 12);
  memcpy(&payload[15], &nodeState.device.distance, 4);

  String payloadStr((char*)payload, 19);
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

void printPositionsAsJSON() {
  Serial.print("{\"n\":[");
  for (uint8_t i = 0; i < 3; i++) {
    Serial.printf("{\"i\":%d,\"x\":%.2f,\"y\":%.2f}", i, nodeState.fixedNodes.posX[i], nodeState.fixedNodes.posY[i]);
    if (i < 2) Serial.print(",");
  }
  Serial.print("],\"d\":{");
  Serial.printf("\"x\":%.2f,\"y\":%.2f,\"r\":%.2f", nodeState.device.x, nodeState.device.y, nodeState.device.distance);
  Serial.println("}}");
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
  computeDevicePosition();
  updateAdvertisingPayload();
  if (!CALIBRATION_MODE) {
    printPositionsAsJSON();
  }
}