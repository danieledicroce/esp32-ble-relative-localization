#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define NODE_ID 16           // ID di questo robot: da DEVICE_ID_BASE a DEVICE_ID_BASE + NUM_DEVICES - 1
#define NUM_FIXED_NODES 3    // numero di ancore (nodi fissi)
#define NUM_DEVICES 10       // numero di robot, identico a quello dello sketch delle ancore
#define DEVICE_ID_BASE 16    // i robot hanno ID DEVICE_ID_BASE .. DEVICE_ID_BASE+NUM_DEVICES-1

/* Coordinate delle ancore in metri, identiche a quelle dello sketch delle ancore
L'ancora 0 e' nell'origine, la 1 sull'asse x (NODE_Y[1] = 0), la 2 con NODE_Y[2] > 0. */
const float NODE_X[NUM_FIXED_NODES] = {0.0, 6.0, 0.0};
const float NODE_Y[NUM_FIXED_NODES] = {0.0, 0.0, 4.0};

#define DISTANCE_MISSING 0xFFFF  // distanza non disponibile nel payload

static_assert(3 + 2 * NUM_DEVICES <= 29, "Troppi robot: il payload non entra nel pacchetto legacy");

struct DeviceState {
  float anchorDistances[NUM_FIXED_NODES];   // distanza del robot da ciascuna ancora, ricevuta dall'ancora
  float x = 0.0;
  float y = 0.0;
  bool located = false;                     // true se la posizione e' stata calcolata
};

struct NodeState {
  DeviceState devices[NUM_DEVICES];
};

NodeState nodeState;

/* Le ancore misurano l'RSSI dei robot e trasmettono le distanze nel loro payload:
questo robot le riceve e ricava dove si trovano tutti i robot, se stesso compreso. */
void handleAnchorData(uint8_t anchorId, const uint8_t *anchorDistances) {
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    uint16_t cm = (uint16_t)anchorDistances[2 * j] | ((uint16_t)anchorDistances[2 * j + 1] << 8);
    nodeState.devices[j].anchorDistances[anchorId] = (cm == DISTANCE_MISSING) ? -1.0 : cm / 100.0;
  }
  /* Serial.printf("[Anchor %d] received %d distances\n", anchorId, NUM_DEVICES); */
}

void trilaterateFromBaseline(float d0, float d1, float d01, float &x, float &y) {
  x = (d0 * d0 - d1 * d1 + d01 * d01) / (2 * d01);
  float ySquared = d0 * d0 - x * x;
  y = (ySquared > 0) ? sqrt(ySquared) : 0.0;
}

void computePositions() {
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    DeviceState &device = nodeState.devices[j];

    device.located = false;
    if (device.anchorDistances[0] < 0 ||
        device.anchorDistances[1] < 0 ||
        device.anchorDistances[2] < 0) continue;   // robot non visto da una delle tre ancore

    float d0 = device.anchorDistances[0];
    float d1 = device.anchorDistances[1];
    float d2 = device.anchorDistances[2];

    float x, yCandidate;
    trilaterateFromBaseline(d0, d1, NODE_X[1], x, yCandidate);

    float x2 = NODE_X[2];
    float y2 = NODE_Y[2];

    float distIfPositive = sqrt(pow(x - x2, 2) + pow(yCandidate - y2, 2));
    float distIfNegative = sqrt(pow(x - x2, 2) + pow(-yCandidate - y2, 2));

    device.x = x;
    device.y = (fabs(distIfPositive - d2) <= fabs(distIfNegative - d2)) ? yCandidate : -yCandidate;
    device.located = true;

    /* Serial.printf("[Robot %d] X: %.2f | Y: %.2f (Chosen Y Candidate: %.2f)\n",
      DEVICE_ID_BASE + j, device.x, device.y, yCandidate); */
  }
}

class ScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      String data = advertisedDevice.getManufacturerData();

      // si considerano solo i pacchetti delle ancore (ID 0, 1, 2): gli altri robot vengono ignorati
      if (data.length() >= 3 + 2 * NUM_DEVICES &&
          (uint8_t)data[0] == 0xFF &&
          (uint8_t)data[1] == 0xFF &&
          (uint8_t)data[2] < NUM_FIXED_NODES) {
        handleAnchorData((uint8_t)data[2], (const uint8_t *)&data[3]);
      }
    }
  }
};

void setupBroadcaster() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advData;
  uint8_t payload[3];
  payload[0] = 0xFF; payload[1] = 0xFF;    // Company ID (test/prototipazione)
  payload[2] = NODE_ID;                    // Node ID

  String payloadStr((char*)payload, 3);
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

void printPositionsAsJSON() {
  Serial.print("{\"n\":[");
  for (uint8_t i = 0; i < NUM_FIXED_NODES; i++) {
    Serial.printf("{\"i\":%d,\"x\":%.2f,\"y\":%.2f}", i, NODE_X[i], NODE_Y[i]);
    if (i < NUM_FIXED_NODES - 1) Serial.print(",");
  }
  Serial.print("],\"d\":[");
  bool first = true;
  for (uint8_t j = 0; j < NUM_DEVICES; j++) {
    DeviceState &device = nodeState.devices[j];
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
      nodeState.devices[j].anchorDistances[i] = -1.0;
    }
  }
  BLEDevice::init("ESP32_ROBOT");
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
  printPositionsAsJSON();
}