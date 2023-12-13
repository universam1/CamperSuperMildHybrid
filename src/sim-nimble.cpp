#include <Arduino.h>

#include "NimBLEDevice.h"
#include "NimBLELog.h"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    log_i("Device connected");
    // uint16_t bleID = pServer->getConnId();
    // auto bleID = pServer->getPeerDevices()[0];
    // // pServer->updatePeerMTU(bleID, 100);

    // log_i("updateMTU to: %i", pServer->getPeerMTU(bleID));
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
// static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
// static BLEUUID charUUID_tx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
// static BLEUUID charUUID_rx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

// static BLEUUID faServiceUUID("0000fa01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
#define serviceUUID "0000ff00-0000-1000-8000-00805f9b34fb"   // xiaoxiang bms original module
#define charUUID_tx "0000ff01-0000-1000-8000-00805f9b34fb"   // xiaoxiang bms original module
#define charUUID_rx "0000ff02-0000-1000-8000-00805f9b34fb"   // xiaoxiang bms original module
#define faServiceUUID "0000fa00-0000-1000-8000-00805f9b34fb" // xiaoxiang bms original module
BLECharacteristic *m_pnpCharacteristic;                      // 0x2a50

const uint8_t basicInfoRequest[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t cellInfoRequest[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
const uint8_t deviceInfoRequest[] = {0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77};
const uint8_t testRequest[] = {0x11};

// DD-03-00-1B-05-1F-00-00-59-0F-59-10-00-00-2A-9A-00-00-00-00
uint8_t basicInfoResponse1[] = {0xDD, 0x03, 0x00, 0x1B, 0x05, 0x1F, 0x00, 0x00, 0x59, 0x0F, 0x59, 0x10, 0x00, 0x00, 0x2A, 0x9A, 0x00, 0x00, 0x00, 0x00};
// 00-00-21-64-03-04-02-0B-0C-0B-09-FD-73-77
uint8_t basicInfoResponse2[] = {0x00, 0x00, 0x21, 0x64, 0x03, 0x04, 0x02, 0x0B, 0x0C, 0x0B, 0x09, 0xFD, 0x73, 0x77};

// DD-03-00-1B-05-1F-00-00-59-0F-59-10-00-00-2A-9A-00-00-00-00 00-00-21-64-03-04-02-0B-0C-0B-09-FD-73-77
// uint8_t basicInfoResponse[] = {0xDD, 0x03, 0x00, 0x1B, 0x05, 0x1F, 0x00, 0x00, 0x59, 0x0F, 0x59, 0x10, 0x00, 0x00, 0x2A, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x64, 0x03, 0x04, 0x02, 0x0B, 0x0C, 0x0B, 0x09, 0xFD, 0x73, 0x77};

// DD 04 00 1E 0F 66 0F 63 0F 63 0F 64 0F 3E 0F 63 0F 37 0F 5B 0F 65 0F 3B 0F 63 0F 63 0F 3C 0F 66 0F 3D F9 F9 77
uint8_t cellInfoResponse[] = {0xDD,0x04,0x00,0x1E,0x0F,0x66,0x0F,0x63,0x0F,0x63,0x0F,0x64,0x0F,0x3E,0x0F,0x63,0x0F,0x37,0x0F,0x5B,0x0F,0x65,0x0F,0x3B,0x0F,0x63,0x0F,0x63,0x0F,0x3C,0x0F,0x66,0x0F,0x3D,0xF9,0xF9,0x77};

// DD-05-00-19-4A-42-44-2D-53-50-30-34-53-30-32-38-2D-4C-34-53
uint8_t deviceInfoResponse1[] = {0xDD, 0x05, 0x00, 0x19, 0x4A, 0x42, 0x44, 0x2D, 0x53, 0x50, 0x30, 0x34, 0x53, 0x30, 0x32, 0x38, 0x2D, 0x4C, 0x34, 0x53};
// 2D-31-35-30-41-2D-42-2D-55-FA-01-77
uint8_t deviceInfoResponse2[] = {0x2D, 0x31, 0x35, 0x30, 0x41, 0x2D, 0x42, 0x2D, 0x55, 0xFA, 0x01, 0x77};
uint8_t splitAt = 20;

// [D][uart_debug:114]: >>> DD:A5:04:00:FF:FC:77
// [D][uart_debug:114]: <<< DD:04:00:08:0F:23:0F:1C:0F:12:0F:1D:FF:4E:77
// [D][uart_debug:114]: >>> DD:A5:03:00:FF:FD:77
// [D][uart_debug:114]: <<< DD:03:00:1D:06:0B:00:00:01:ED:01:F4:00:00:2C:7C:00:00:00:00:10:00:80:63:02:04:03:0B:A0:0B:9D:0B:98:FA:55:77

class MyRXCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {

    // std::string rxValue = pCharacteristic->getValue();
    auto rxValue = pCharacteristic->getValue();
    auto len = pCharacteristic->getDataLength();
    // auto len = rxValue.size();
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

      txChar->setValue(basicInfoResponse1, sizeof(basicInfoResponse1));
      txChar->notify();
      // delay(10);
      txChar->setValue(basicInfoResponse2, sizeof(basicInfoResponse2));
      txChar->notify();
      // txChar->setValue(basicInfoResponse, sizeof(basicInfoResponse));
      // txChar->notify();
    }
    else if (memcmp(rxValue, cellInfoRequest, len) == 0)
    {
      Serial.println("cell info request");
      txChar->setValue(cellInfoResponse, 20);
      txChar->notify();
      txChar->setValue(cellInfoResponse+20, sizeof(cellInfoResponse)-20);
      txChar->notify();
    }
    else if (memcmp(rxValue, deviceInfoRequest, len) == 0)
    {
      Serial.println("device info request");
      txChar->setValue(deviceInfoResponse1, sizeof(deviceInfoResponse1));
      txChar->notify();
      txChar->setValue(deviceInfoResponse2, sizeof(deviceInfoResponse2));
      txChar->notify();
    }
    else
    {

      Serial.println("unknown request");
      txChar->setValue(basicInfoResponse1, sizeof(basicInfoResponse1));
      txChar->notify();
      txChar->setValue(basicInfoResponse2, sizeof(basicInfoResponse2));
      txChar->notify();
    }
  }
};

class MyTXCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    // std::string rxValue = pCharacteristic->getValue();
    auto rxValue = pCharacteristic->getValue();
    auto len = pCharacteristic->getDataLength();
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
  NimBLEDevice::init("BMSNimClone");
  NimBLEDevice::setMTU(48);
  // Create the BLE Server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *m_deviceInfoService = pServer->createService(NimBLEUUID((uint16_t)0x180a));
  BLECharacteristic *m_pnpCharacteristic = m_deviceInfoService->createCharacteristic((uint16_t)0x2a50, NIMBLE_PROPERTY::READ);
  // uint8_t pnp[] = {sig, (uint8_t)(vid >> 8), (uint8_t)vid, (uint8_t)(pid >> 8), (uint8_t)pid, (uint8_t)(version >> 8), (uint8_t)version};
  // m_pnpCharacteristic->setValue(pnp, sizeof(pnp));

  // Create the BLE Service
  BLEService *pService = pServer->createService(serviceUUID);

  // 0000ff01-0000-1000-8000-00805f9b34fb
  // NOTIFY, READ
  // Notifications from this characteristic is received data from BMS
  pTxCharacteristic = pService->createCharacteristic(
      charUUID_tx,
      NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  pTxCharacteristic->setCallbacks(new MyTXCallbacks());
  // pTxCharacteristic->addDescriptor(new BLE2902());

  // BLEDescriptor *p2901Descriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901)); // Characteristic User Description
  // p2901Descriptor->setValue("xiaoxiang bms");

  NimBLEDescriptor *pTx2901Descriptor = pTxCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ);
  // pTx2901Descriptor->setValue("Power");
  // pTxCharacteristic->addDescriptor(pTx2901Descriptor);

  // 0000ff02-0000-1000-8000-00805f9b34fb
  // Write this characteristic to send data to BMS
  // READ, WRITE, WRITE NO RESPONSE
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      charUUID_rx,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE);
  pRxCharacteristic->setCallbacks(new MyRXCallbacks());
  // pRxCharacteristic->addDescriptor(new BLE2902());

  NimBLEDescriptor *pRx2901Descriptor = pRxCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ);
  // pRx2901Descriptor->setValue("Power");
  // pRxCharacteristic->addDescriptor(pRx2901Descriptor);

  // Create the FA service
  BLEService *pFaService = pServer->createService(faServiceUUID);

  // Create a BLE Characteristic
  BLECharacteristic *pFaCharacteristic = pFaService->createCharacteristic(
      "0000fa01-0000-1000-8000-00805f9b34fb",
      NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  pFaCharacteristic->setCallbacks(new MyTXCallbacks());
  NimBLEDescriptor *pFa2901Descriptor = pFaCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ);

  // Start the service
  pService->start();
  pFaService->start();
  m_deviceInfoService->start();

  // Start advertising

  // BLEAdvertisementData advertisementData;
  char manData[] = {0x6c, 0x97, 0xe6, 0x38, 0xc1, 0xa4};
  // advertisementData.setManufacturerData(std::string(manData, sizeof manData));
  BLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  // pAdvertising->setAdvertisementData(advertisementData);
  pAdvertising->setManufacturerData(std::string(manData, sizeof manData));
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->addServiceUUID(pFaService->getUUID());
  pAdvertising->addServiceUUID(m_deviceInfoService->getUUID());
  pAdvertising->setScanResponse(true);
  // pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  // pAdvertising->setMaxPreferred(0x12);
  // NimBLEDevice::startAdvertising();
  pAdvertising->start();
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
