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

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("connection");
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("disconnect");
    deviceConnected = false;
  }
};
// 0000ff01-0000-1000-8000-00805f9b34fb
// NOTIFY, READ
// Notifications from this characteristic is received data from BMS

// 0000ff02-0000-1000-8000-00805f9b34fb
// Write this characteristic to send data to BMS
// READ, WRITE, WRITE NO RESPONSE
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

static BLEUUID faServiceUUID("0000fa01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

BLECharacteristic *m_pnpCharacteristic; // 0x2a50

class MyRXCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {

uint8_t basicInfoRequest[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
uint8_t cellInfoRequest[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
uint8_t deviceInfoRequest[] = {0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77};
uint8_t testRequest[] = {0x11};
    // {DD A5 00 1B 13 78 00 00 00 00 03 E8 00 00 22 C7 00 00 00 00 00 00 19 00 03 0C 02 0B 64 0B 5F FC 83 77};
    uint8_t basicInfoResponse[] = {0xDD, 0xA5, 0x00, 0x1B, 0x13, 0x78, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8, 0x00, 0x00, 0x22, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x03, 0x0C, 0x02, 0x0B, 0x64, 0x0B, 0x5F, 0xFC, 0x83, 0x77};
    // DD A5 00 18 10 39 10 3A 10 38 10 3A 10 3C 10 39 10 37 10 39 10 3B 10 3F 10 36 10 3A FC 74 77
    uint8_t cellInfoResponse[] = {0xDD, 0xA5, 0x00, 0x18, 0x10, 0x39, 0x10, 0x3A, 0x10, 0x38, 0x10, 0x3A, 0x10, 0x3C, 0x10, 0x39, 0x10, 0x37, 0x10, 0x39, 0x10, 0x3B, 0x10, 0x3F, 0x10, 0x36, 0x10, 0x3A, 0xFC, 0x74, 0x77};
    // DD A5 00 14 4C 48 2D 53 50 31 35 53 30 30 31 2D 50 31 33 53 2D 33 30 41 FB 39 77
    uint8_t deviceInfoResponse[] = {0xDD, 0xA5, 0x00, 0x14, 0x4C, 0x48, 0x2D, 0x53, 0x50, 0x31, 0x35, 0x53, 0x30, 0x30, 0x31, 0x2D, 0x50, 0x31, 0x33, 0x53, 0x2D, 0x33, 0x30, 0x41, 0xFB, 0x39, 0x77};
    // std::string rxValue = pCharacteristic->getValue();
    auto rxValue = pCharacteristic->getData();
    auto len = pCharacteristic->getLength();
    if (len > 0)
    {
      Serial.println("*********");
      Serial.print("Received RX: ");
      // Serial.println(pCharacteristic->getUUID().toString().c_str());
      // param->write.value;
      for (int i = 0; i < len; i++)
      {
        Serial.print("0x");
        Serial.print(rxValue[i] < 16 ? "0" : "");
        Serial.print(rxValue[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      Serial.println("*********");
    }

    BLECharacteristic *txChar = pServer->getServiceByUUID(serviceUUID)->getCharacteristic(charUUID_tx);
    if (txChar == NULL)
    {
      Serial.println("txChar not found");
      return;
    }

    if (memcmp(rxValue, basicInfoRequest, len) == 0)
    {
      Serial.println("basic info request");

      txChar->setValue(&basicInfoResponse[0], sizeof(basicInfoResponse));
      txChar->notify();
    }
    else if (memcmp(rxValue, cellInfoRequest, len) == 0)
    {
      Serial.println("cell info request");
      txChar->setValue(&cellInfoResponse[0], sizeof(cellInfoResponse));
      txChar->notify();
    }
    else if (memcmp(rxValue, deviceInfoRequest, len) == 0)
    {
      Serial.println("device info request");
      txChar->setValue(&deviceInfoResponse[0], sizeof(deviceInfoResponse));
      txChar->notify();
    }
    else if (memcmp(rxValue, testRequest, len) == 0)
    {
      // pCharacteristic->setValue(rxValue, len);
      // pCharacteristic->notify();
      txChar->setValue(&basicInfoResponse[0], sizeof(basicInfoResponse));
      txChar->notify();
    }

    else
    {
      txChar->setValue(rxValue, len);
      txChar->notify();
      Serial.println("unknown request");
    }
  }
};

class MyTXCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    // std::string rxValue = pCharacteristic->getValue();
    auto rxValue = pCharacteristic->getData();
    auto len = pCharacteristic->getLength();
    if (len > 0)
    {
      Serial.println("*********");
      Serial.print("Received TX: ");
      Serial.println(pCharacteristic->getUUID().toString().c_str());
      for (int i = 0; i < len; i++)
      {
        Serial.print("0x");
        Serial.print(rxValue[i] < 16 ? "0" : "");
        Serial.print(rxValue[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      Serial.println("*********");
    }
  }
};

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
  BLEDevice::init("BMSClone");
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
  BLEService *pService = pServer->createService(BLEUUID((uint16_t)0xff00));

  BLEDescriptor *p2901Descriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901)); // Characteristic User Description
  p2901Descriptor->setValue("xiaoxiang bms");

  // 0000ff01-0000-1000-8000-00805f9b34fb
  // NOTIFY, READ
  // Notifications from this characteristic is received data from BMS
  pTxCharacteristic = pService->createCharacteristic(
      BLEUUID((uint16_t)0xff01),
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pTxCharacteristic->setCallbacks(new MyTXCallbacks());
  pTxCharacteristic->addDescriptor(new BLE2902());
  pTxCharacteristic->addDescriptor(p2901Descriptor);

  // 0000ff02-0000-1000-8000-00805f9b34fb
  // Write this characteristic to send data to BMS
  // READ, WRITE, WRITE NO RESPONSE
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      BLEUUID((uint16_t)0xff02),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxCharacteristic->setCallbacks(new MyRXCallbacks());
  // pRxCharacteristic->addDescriptor(new BLE2902());
  pRxCharacteristic->addDescriptor(p2901Descriptor);

  // Create the FA service
  BLEService *pFaService = pServer->createService(faServiceUUID);

  // Create a BLE Characteristic
  BLECharacteristic *pFaCharacteristic = pFaService->createCharacteristic(
      BLEUUID((uint16_t)0xfa01),
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pFaCharacteristic->setCallbacks(new MyTXCallbacks());
  pFaCharacteristic->addDescriptor(p2901Descriptor);

  // Start the service
  pService->start();
  pFaService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData advertisementData;
  // char sNameData[] = {0x00, 0xff, 0x28, 0x50};
  // advertisementData.setShortName(std::string(sNameData, sizeof sNameData));

  char manData[] = {0x6c, 0x97, 0xe6, 0x38, 0xc1, 0xa4};
  advertisementData.setManufacturerData(std::string(manData, sizeof manData));
  pAdvertising->setAdvertisementData(advertisementData);
  // pAdvertising->setScanResponse(true);
  // pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  // pAdvertising->setMinPreferred(0x12);
  pAdvertising->addServiceUUID(serviceUUID);
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

  // pTxCharacteristic->setValue(basicInfoResponse, sizeof(basicInfoResponse));
  // pTxCharacteristic->notify();
  // Serial.println("basic info send");
}

void loop()
{

  if (deviceConnected)
  {
    // Serial.println("connection");

    // pTxCharacteristic->setValue(&txValue, 1);
    // pTxCharacteristic->notify();
    // txValue++;
    // delay(10); // bluetooth stack will go into congestion, if too many packets are sent
    // switch (sendInfo)
    // {
    // case 1:
    //   Serial.println("basic info send");
    //   // pTxCharacteristic->setValue(basicInfoResponse, sizeof(basicInfoResponse));
    //   // pTxCharacteristic->notify();
    //   sendInfo = 0;
    //   break;

    // case 2:
    //   Serial.println("cell info send");
    //   // pTxCharacteristic->setValue(cellInfoResponse, sizeof(cellInfoResponse));
    //   // pTxCharacteristic->notify();
    //   sendInfo = 0;
    //   break;

    // case 3:
    //   Serial.println("device info send");
    //   // pTxCharacteristic->setValue(deviceInfoResponse, sizeof(deviceInfoResponse));
    //   // pTxCharacteristic->notify();
    //   sendInfo = 0;
    //   break;
    // }
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
    Serial.println("connection");
  }
}
