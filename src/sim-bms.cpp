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
    log_i("Device connected");
    uint16_t bleID = pServer->getConnId();
    pServer->updatePeerMTU(bleID, 100);
    log_i("updateMTU to: %i", pServer->getPeerMTU(bleID));
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

const uint8_t basicInfoRequest[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t cellInfoRequest[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
const uint8_t deviceInfoRequest[] = {0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77};
const uint8_t testRequest[] = {0x11};

// DD-03-00-1B-05-1F-00-00-59-0F-59-10-00-00-2A-9A-00-00-00-00 00-00-21-64-03-04-02-0B-0C-0B-09-FD-73-77
uint8_t basicInfoResponse[] = {0xDD, 0x03, 0x00, 0x1B, 0x05, 0x1F, 0x00, 0x00, 0x59, 0x0F, 0x59, 0x10, 0x00, 0x00, 0x2A, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x64, 0x03, 0x04, 0x02, 0x0B, 0x0C, 0x0B, 0x09, 0xFD, 0x73, 0x77};

uint8_t cellInfoResponse[] = {0xDD,0x04,0x00,0x1E,0x0F,0x66,0x0F,0x63,0x0F,0x63,0x0F,0x64,0x0F,0x3E,0x0F,0x63,0x0F,0x37,0x0F,0x5B,0x0F,0x65,0x0F,0x3B,0x0F,0x63,0x0F,0x63,0x0F,0x3C,0x0F,0x66,0x0F,0x3D,0xF9,0xF9,0x77};

// DD-05-00-19-4A-42-44-2D-53-50-30-34-53-30-32-38-2D-4C-34-53 2D-31-35-30-41-2D-42-2D-55-FA-01-77
uint8_t deviceInfoResponse[] = {0xDD, 0x05, 0x00, 0x19, 0x4A, 0x42, 0x44, 0x2D, 0x53, 0x50, 0x30, 0x34, 0x53, 0x30, 0x32, 0x38, 0x2D, 0x4C, 0x34, 0x53, 0x2D, 0x31, 0x35, 0x30, 0x41, 0x2D, 0x42, 0x2D, 0x55, 0xFA, 0x01, 0x77};

uint8_t splitAt = 20;
class MyRXCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {

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

      txChar->setValue(&basicInfoResponse[0], splitAt);
      txChar->notify();
      log_i("splitAt 1: %i", splitAt);
      delay(100);
      while (pServer->getConnectedCount() == 0)
      {
        log_e("no connection");
      }
      log_i("connected: %d", pServer->getPeerDevices(true).size());

      txChar->setValue(basicInfoResponse + splitAt, sizeof(basicInfoResponse) - splitAt);
      txChar->notify();
      log_i("splitAt 2: %i", sizeof(basicInfoResponse) - splitAt);
    }
    else if (memcmp(rxValue, cellInfoRequest, len) == 0)
    {
      Serial.println("cell info request");
      // txChar->setValue(&cellInfoResponse[0], splitAt);
      // txChar->notify();
      // txChar->setValue(&cellInfoResponse[splitAt], sizeof(cellInfoResponse) - splitAt);
      txChar->setValue(cellInfoResponse, sizeof(cellInfoResponse));
      txChar->notify();
    }
    else if (memcmp(rxValue, deviceInfoRequest, len) == 0)
    {
      Serial.println("device info request");
      txChar->setValue(&deviceInfoResponse[0], splitAt);
      txChar->notify();
      txChar->setValue(&deviceInfoResponse[splitAt], sizeof(deviceInfoResponse) - splitAt);
      txChar->notify();
    }
    else
    {

      Serial.println("unknown request");
      // splitAt = *rxValue;
      txChar->setValue(basicInfoResponse, splitAt);
      txChar->notify();
      log_i("splitAt 1: %i", splitAt);
      delay(10);
      txChar->setValue(basicInfoResponse + splitAt, sizeof(basicInfoResponse) - splitAt);
      txChar->notify();
      log_i("splitAt 2: %i", sizeof(basicInfoResponse) - splitAt);
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
  BLEDevice::setMTU(100);
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
