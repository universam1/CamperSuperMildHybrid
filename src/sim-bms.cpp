/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE"
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second.
*/
#include <Arduino.h>
/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE"
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

// #define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
// #define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
// #define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
// const char *BLE_SERIAL_SERVICE_UUID = "ff00";
// const char *BLE_RX_UUID = "ff01";
// const char *BLE_TX_UUID = "ff02";
// BLEUUID serviceUUID;
// BLEUUID charRxUUID;
// BLEUUID charTxUUID;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0)
    {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++)
        Serial.print(rxValue[i]);

      Serial.println();
      Serial.println("*********");
    }
  }
};
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
BLECharacteristic *m_pnpCharacteristic;                             // 0x2a50
void setup()
{
  Serial.begin(115200);
  // serviceUUID = BLEUUID().fromString(BLE_SERIAL_SERVICE_UUID);
  // charRxUUID = BLEUUID().fromString(BLE_RX_UUID);
  // charTxUUID = BLEUUID().fromString(BLE_TX_UUID);
  // auto t = serviceUUID.toString();
  // Serial.println(t.c_str());
  // Serial.println(charRxUUID.toString().c_str());
  // Serial.println(charTxUUID.toString().c_str());
  // Create the BLE Device
  BLEDevice::init("BMS Clone");
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  /*
   * Here we create mandatory services described in bluetooth specification
   */
  BLEService *m_deviceInfoService = pServer->createService(BLEUUID((uint16_t)0x180a));

  /*
   * Mandatory characteristic for device info service
   */
  BLECharacteristic *m_pnpCharacteristic = m_deviceInfoService->createCharacteristic((uint16_t)0x2a50, BLECharacteristic::PROPERTY_READ);
  // uint8_t pnp[] = {sig, (uint8_t)(vid >> 8), (uint8_t)vid, (uint8_t)(pid >> 8), (uint8_t)pid, (uint8_t)(version >> 8), (uint8_t)version};
  // m_pnpCharacteristic->setValue(pnp, sizeof(pnp));

  // Create the BLE Service
  BLEService *pService = pServer->createService(serviceUUID);

  BLEDescriptor *p2901Descriptor = new BLEDescriptor((uint16_t)0x2901); // Characteristic User Description

  // Create a BLE Characteristic
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      charUUID_rx, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  pRxCharacteristic->addDescriptor(new BLE2902());
  pRxCharacteristic->addDescriptor(p2901Descriptor);

  pTxCharacteristic = pService->createCharacteristic(
      charUUID_tx, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE_NR);
  pTxCharacteristic->addDescriptor(new BLE2902());
  pTxCharacteristic->addDescriptor(p2901Descriptor);

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  BLEAdvertisementData advertisementData;
  // char sNameData[] = {0x00, 0xff, 0x28, 0x50};
  // advertisementData.setShortName(std::string(sNameData, sizeof sNameData));

  char manData[] = {0x6c, 0x97, 0xe6, 0x38, 0xc1, 0xa4};
  advertisementData.setManufacturerData(std::string(manData, sizeof manData));
  pAdvertising->setAdvertisementData(advertisementData);

  pAdvertising->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop()
{

  if (deviceConnected)
  {
    pTxCharacteristic->setValue(&txValue, 1);
    pTxCharacteristic->notify();
    txValue++;
    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
