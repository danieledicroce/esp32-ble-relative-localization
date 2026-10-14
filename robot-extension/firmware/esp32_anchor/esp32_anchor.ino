#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define NODE_ID 0            // 0 .. NUM_FIXED_NODES-1 a seconda della scheda
#define NUM_FIXED_NODES 3    // numero di nodi fissi
#define NUM_DEVICES 10       // numero di robot da localizzare (al massimo 13)
#define DEVICE_ID_BASE 16    // i robot hanno ID DEVICE_ID_BASE .. DEVICE_ID_BASE+NUM_DEVICES-1

/* Coordinate dei nodi fissi in metri, misurate a mano, nello stesso sistema di riferimento per tutti.
Il nodo 0 e' nell'origine, il nodo 1 sull'asse x (NODE_Y[1] = 0), il nodo 2 con NODE_Y[2] > 0
e non allineato ai primi due. */
const float NODE_X[NUM_FIXED_NODES] = {0.0, 6.0, 3.0};
const float NODE_Y[NUM_FIXED_NODES] = {0.0, 0.0, 4.0};

#define RSSI_AT_1M -56.0        // RSSI(d0), da ricalibrare per ogni ambiente
#define PATH_LOSS_EXPONENT 3.0  // n, idem
#define PROCESS_NOISE 2.0       // Q, piu' alto del prototipo (0.5): valore atteso per robot in movimento
#define MEASUREMENT_NOISE 10.0  // R, idem

#define CALIBRATION_MODE false

#define DISTANCE_MISSING 0xFFFF  // distanza non disponibile nel payload

static_assert(3 + 2 * NUM_DEVICES <= 29, "Troppi robot: il payload non entra nel pacchetto legacy");

struct KalmanState {
  float estimate = 0.0;
  float errorCovariance = 1.0;
  bool initialized = false;
};

struct ExternalDeviceState {
  float distance = -1.0;                          // propria distanza misurata dal robot
  float peerDistances[NUM_FIXED_NODES];           // distanza dal robot per ciascun nodo fisso, propria inclusa
  float x = 0.0;
  float y = 0.0;
  bool located = false;                           // true se la posizione e' stata calcolata
  KalmanState kalman;
};

struct NodeState {
  ExternalDeviceState devices[NUM_DEVICES];
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

void handleRSSIReading(uint8_t deviceIndex, int rssi) {
  ExternalDeviceState &device = nodeState.devices[deviceIndex];
  int filteredRSSI = kalmanFilterRSSI(device.kalman, rssi);
  device.distance = distanceFromRSSI(filteredRSSI);
  device.peerDistances[NODE_ID] = device.distance;

  if (CALIBRATION_MODE) {
    Serial.printf("[Device %d] Raw RSSI: %d | Filtered RSSI: %d -> Dist: %.2f\n",
      DEVICE_ID_BASE + deviceIndex, rssi, filteredRSSI, device.distance);
  }
}

void handlePeerData(uint8_t senderId, const uint8_t *peerDeviceDistances) {
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    uint16_t cm = (uint16_t)peerDeviceDistances[2 * j] | ((uint16_t)peerDeviceDistances[2 * j + 1] << 8);
    /* Esempio: una distanza di 350 cm vale 0x015E, quindi nel pacchetto arrivano i byte 0x5E e 0x01.
       0x01 << 8 dà 0x0100 (256)
       0x0100 | 0x5E dà 0x015E (350) */

    nodeState.devices[j].peerDistances[senderId] = (cm == DISTANCE_MISSING) ? -1.0 : cm / 100.0;
  }
  /* Serial.printf("[Peer %d] received %d distances\n", senderId, NUM_DEVICES); */
}

void trilaterateFromBaseline(float d0, float d1, float d01, float &x, float &y) {
  x = (d0 * d0 - d1 * d1 + d01 * d01) / (2 * d01);
  float ySquared = d0 * d0 - x * x;
  y = (ySquared > 0) ? sqrt(ySquared) : 0.0;
}

void computeDevicePosition() {
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    ExternalDeviceState &device = nodeState.devices[j];

    device.located = false;
    if (device.peerDistances[0] < 0 ||
        device.peerDistances[1] < 0 ||
        device.peerDistances[2] < 0) continue;   // robot non visto da uno dei tre nodi

    float d0 = device.peerDistances[0];
    float d1 = device.peerDistances[1];
    float d2 = device.peerDistances[2];

    float x, yCandidate;
    trilaterateFromBaseline(d0, d1, NODE_X[1], x, yCandidate);

    float x2 = NODE_X[2];
    float y2 = NODE_Y[2];

    float distIfPositive = sqrt(pow(x - x2, 2) + pow(yCandidate - y2, 2));
    float distIfNegative = sqrt(pow(x - x2, 2) + pow(-yCandidate - y2, 2));

    device.x = x;
    device.y = (fabs(distIfPositive - d2) <= fabs(distIfNegative - d2)) ? yCandidate : -yCandidate;
    device.located = true;

    /* Serial.printf("[Target Position %d] X: %.2f | Y: %.2f (Chosen Y Candidate: %.2f)\n",
      DEVICE_ID_BASE + j, device.x, device.y, yCandidate); */
  }
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

        if (senderId >= DEVICE_ID_BASE && senderId < DEVICE_ID_BASE + NUM_DEVICES) {
          handleRSSIReading(senderId - DEVICE_ID_BASE, rssi);

        } else if (senderId < NUM_FIXED_NODES && senderId != NODE_ID &&
                   data.length() >= 3 + 2 * NUM_DEVICES) {
          handlePeerData(senderId, (const uint8_t *)&data[3]);
        }
      }
    }
  }
};

void updateAdvertisingPayload() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advData;
  uint8_t payload[3 + 2 * NUM_DEVICES];
  payload[0] = 0xFF; payload[1] = 0xFF;    // Company ID (test/prototipazione)
  payload[2] = NODE_ID;                    // Node ID
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    float distance = nodeState.devices[j].distance;
    uint16_t cm = DISTANCE_MISSING;
    if (distance >= 0) {
      float value = distance * 100.0 + 0.5;
      cm = (value > 65534.0) ? 65534 : (uint16_t)value;
    }
    payload[3 + 2 * j] = cm & 0xFF;        // distanza in centimetri, 2 byte
    payload[4 + 2 * j] = cm >> 8;
    /* Esempio: 350 cm = 0x015E:
       0x015E & 0xFF dà 0x5E
       0x015E >> 8 dà 0x01 */
  }

  String payloadStr((char*)payload, 3 + 2 * NUM_DEVICES);
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
  for (uint8_t i = 0; i < NUM_FIXED_NODES; i++) {
    Serial.printf("{\"i\":%d,\"x\":%.2f,\"y\":%.2f}", i, NODE_X[i], NODE_Y[i]);
    if (i < NUM_FIXED_NODES - 1) Serial.print(",");
  }
  Serial.print("],\"d\":[");
  bool first = true;
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    ExternalDeviceState &device = nodeState.devices[j];
    if (!device.located) continue;
    if (!first) Serial.print(",");
    first = false;
    Serial.printf("{\"i\":%d,\"x\":%.2f,\"y\":%.2f}", DEVICE_ID_BASE + j, device.x, device.y);
  }
  Serial.println("]}");
}

void setup() {
  Serial.begin(115200);
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    for (uint8_t i = 0; i < NUM_FIXED_NODES; i++) {
      nodeState.devices[j].peerDistances[i] = -1.0;
    }
  }
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

  computeDevicePosition();
  updateAdvertisingPayload();
  if (!CALIBRATION_MODE) {
    printPositionsAsJSON();
  }
}
