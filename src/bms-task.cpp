#include "smarthybrid.h"
#include <BLEDevice.h>

// #define SIMULATE_BMS
#define POLLING_INTERVAL_MS 300
BMSInfo_t bmsInfo;

// header status command length data calcChecksum footer
//   DD     A5      03     00    FF     FD      77
uint8_t basicRequest[] = {0xdd, 0xa5, 0x3, 0x0, 0xff, 0xfd, 0x77};
uint8_t cellInfoRequest[] = {0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77};

TaskHandle_t vBMS_Task_hdl, vBMS_Scan_hdl, vBMS_Polling_hdl;
BLERemoteCharacteristic *pTxRemoteCharacteristic;
BLEAdvertisedDevice *bmsDevice = nullptr;
BLEClient *pClient = nullptr;
bool connectToServer(BLEAdvertisedDevice advertisedDevice);
// The remote service we wish to connect to. Needs check/change when other BLE module used.
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
// 0000ff01-0000-1000-8000-00805f9b34fb
// NOTIFY, READ
// Notifications from this characteristic is received data from BMS

// 0000ff02-0000-1000-8000-00805f9b34fb
// Write this characteristic to send data to BMS
// READ, WRITE, WRITE NO RESPONSE

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    log_d("BLE Advertised Device found: %s", advertisedDevice.toString().c_str());
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {
      // vTaskSuspend(vBMS_Scan_hdl);
      BLEDevice::getScan()->stop();
      log_d("Found serviceUUID service");
      bmsDevice = new BLEAdvertisedDevice(advertisedDevice);
      // if (connectToServer(advertisedDevice))
      // {
      //   log_d("connected to the BLE Server.");
      // }
      // else
      // {
      //   log_d("failed to connect to the server; restart the scan.");
      //   vTaskResume(vBMS_Scan_hdl);
      // }
    }
    else
    {
      log_d("peer not advertising our service");
    }
  }
};

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient)
  {
    log_d("onConnect");
    // vTaskSuspend(vBMS_Scan_hdl);
    vTaskResume(vBMS_Polling_hdl);
  }

  void onDisconnect(BLEClient *pclient)
  {
    log_d("onDisconnect");
    bmsDevice = nullptr;
    vTaskSuspend(vBMS_Polling_hdl);
    // vTaskResume(vBMS_Scan_hdl);
  }
};

// into circular buffer
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  intoCircularBuffer(pData, length);
}

bool connectToServer()
{
  if (bmsDevice == nullptr)
    return false;

  if (pClient->isConnected())
  {
    log_d("already connected");
    return true;
  }

  log_d("Forming a connection to %s", bmsDevice->getAddress().toString().c_str());
  // Connect to the remote BLE Server.
  BLEAddress address = bmsDevice->getAddress();
  esp_ble_addr_type_t type = bmsDevice->getAddressType();
  pClient->connect(address, type); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  log_d(" - Connected to server");
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    log_e("Failed to find our service UUID: %s", serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  log_d(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
  if (pRemoteCharacteristic == nullptr)
  {
    log_e("Failed to find our RX characteristic UUID: %s", charUUID_rx.toString().c_str());
    pClient->disconnect();
    return false;
  }
  log_d(" - Found our RX characteristic");
  if (pRemoteCharacteristic->canRead())
    log_d("The characteristic value was: %s", pRemoteCharacteristic->readValue().c_str());

  if (!pRemoteCharacteristic->canNotify())
  {
    log_e("can't notify RX characteristic");
    return false;
  }
  pRemoteCharacteristic->registerForNotify(notifyCallback);

  // create handle for TX characteristic
  pTxRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);
  if (pTxRemoteCharacteristic == nullptr)
  {
    log_e("Failed to find our TX characteristic UUID: %s", charUUID_tx.toString().c_str());
    pClient->disconnect();
    pTxRemoteCharacteristic = nullptr;
    return false;
  }
  log_d(" - Found our TX characteristic");
  // Read the value of the characteristic.
  if (!pTxRemoteCharacteristic->canRead())
  {
    pTxRemoteCharacteristic = nullptr;
    log_e("can't read TX characteristic");
    return false;
  }
  log_d("The TX characteristic value was: %s", pTxRemoteCharacteristic->readValue().c_str());

  return true;
}

void vBMS_Polling(void *parameter)
{
  vTaskSuspend(NULL); // suspend self until connected
  byte idx = 0;
  while (true)
  {
    if (pTxRemoteCharacteristic == nullptr)
    {
      log_e("Remote TX characteristic unavailable");
      vTaskDelay(pdMS_TO_TICKS(3000));
      continue;
    }

    if (++idx % 4)
      pTxRemoteCharacteristic->writeValue(basicRequest, sizeof(basicRequest));
    else
      pTxRemoteCharacteristic->writeValue(cellInfoRequest, sizeof(cellInfoRequest));

    vTaskDelay(pdMS_TO_TICKS(POLLING_INTERVAL_MS));
  }
  vTaskDelete(NULL);
}

// void set_0xE1_mosfet_control_charge(bool charge) {
//     uint8_t   m_0xE1_mosfet_control[2];

//     if (charge) {
//         m_0xE1_mosfet_control[1] &= 0b10;  // Disable bit zero
//     }
//     else {
//         m_0xE1_mosfet_control[1] |= 0b01;  // Enable bit zero
//     }
//     write(BMS_WRITE, BMS_REG_CTL_MOSFET, m_0xE1_mosfet_control, 2);
// }

void vBMS_Scan(void *parameter)
{
  BLEDevice::init(myName);
  log_d("vBMS_Scan: %d", xPortGetCoreID());
  pClient = BLEDevice::createClient();
  BLEClient *pClient = BLEDevice::createClient();
  log_d(" - Created client");
  pClient->setClientCallbacks(new MyClientCallback());

  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  while (true)
  {
    if (connectToServer())
    {
      log_d("connected to the BLE Server.");
      vTaskResume(vBMS_Polling_hdl);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    log_d("failed to connect to the server; restart the scan.");
    bmsDevice = nullptr;

    BLEScanResults foundDevices = pBLEScan->start(5);
    log_d("scan results: %d", foundDevices.getCount());
    for (int i = 0; i < foundDevices.getCount(); i++)
    {
      BLEAdvertisedDevice device = foundDevices.getDevice(i);
      log_d("Device %d: %s", i, device.toString().c_str());
    }
  }
  vTaskDelete(NULL);
}

void BMSStart()
{
  xTaskCreatePinnedToCore(vBMS_Scan, "SCAN", 5000, NULL, 2, &vBMS_Scan_hdl, 0);
  xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_INIT_BIT, eSetBits);
  xTaskCreatePinnedToCore(vBMS_Polling, "POLL", 5000, NULL, 2, &vBMS_Polling_hdl, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(vBMSProcessTask, "BMSProcess", 5000, NULL, 2, &vBMSProcess_Task_hdl, tskNO_AFFINITY);
}

//   while (true)
//   {
// #ifndef SIMULATE_BMS
//     BLEClient.bleLoop();
//     if (con)
//     {
//       log_d("BLEClient connected");
//       bms.main_task(true);
// #else
//     {
// #endif
//       if (millis() - m_last_basic_info_query_time >= basic_info_query_rate)
//       {
// #ifndef SIMULATE_BMS
//         // bms.query_0x03_basic_info();
//         bmsInfo.Voltage = bms.get_voltage();
//         bmsInfo.Current = bms.get_current();
// #else
//         bmsInfo.Voltage = random(12800, 14600) / 1000.0;
//         bmsInfo.Current = random(-1800, 600) / 1000.0;
// #endif
//         bmsInfo.Power = bmsInfo.Voltage * bmsInfo.Current;
//         log_d("BMS Info: %.2fV %.2fA %.2fW",
//               bmsInfo.Voltage, bmsInfo.Current, bmsInfo.Power);
//         xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits);
//         m_last_basic_info_query_time = millis();
//       }

//       if (millis() - m_last_cell_voltages_query_time >= cell_query_rate)
//       {
// #ifndef SIMULATE_BMS
//         // bms.query_0x04_cell_voltages();
// #endif
//         for (size_t i = 0; i < 4; i++)
// #ifndef SIMULATE_BMS
//           bmsInfo.CellVoltages[i] = bms.get_cell_voltage(i);
// #else
//           bmsInfo.CellVoltages[i] = random(3100, 3300) / 1000.0;
// #endif
//         xTaskNotify(vTFT_Task_hdl, NotificationBits::CELL_UPDATE_BIT, eSetBits);
//         m_last_cell_voltages_query_time = millis();
//       }
//       vTaskDelay(pdMS_TO_TICKS(10));
//     }
// #ifndef SIMULATE_BMS
//     else
//     {
//       log_e("BLEClient not connected");
//       vTaskDelay(pdMS_TO_TICKS(3000));
//     }
// #endif
//   }

//   vTaskDelete(NULL);
// }
