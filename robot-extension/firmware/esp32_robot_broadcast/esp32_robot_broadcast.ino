#include <BLEDevice.h>
#include <BLEAdvertising.h>

#define NODE_ID 16  // identificativo del robot, diverso per ogni scheda (16, 17, 18, ...)

void updateAdvertisingPayload() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advData;
  uint8_t payload[3];
  payload[0] = 0xFF; payload[1] = 0xFF;    // Company ID (test/prototipazione)
  payload[2] = NODE_ID;                    // Node ID

  String payloadStr((char*)payload, 3);
  advData.setManufacturerData(payloadStr);
  pAdvertising->setAdvertisementData(advData);
}

void setupBroadcaster() {
  updateAdvertisingPayload();
  BLEDevice::getAdvertising()->start();
}

void setup() {
  BLEDevice::init("ESP32_NODE");
  setupBroadcaster();
}

void loop() {
}
