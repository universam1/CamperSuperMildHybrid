#include "smarthybrid.h"
#include <BLEDevice.h>

// #define SIMULATE_BMS
BMSInfo_t bmsInfo;

TaskHandle_t vBMS_Task_hdl, vBMS_Scan_hdl, vBMS_Polling_hdl;
BLERemoteCharacteristic *pTxRemoteCharacteristic;
BLEAdvertisedDevice *bmsDevice = nullptr;
BLEClient *pClient = nullptr;

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
    }
    else
      log_d("peer not advertising our service");
  }
};

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient)
  {
    log_d("resume polling");
    vTaskResume(vBMS_Polling_hdl);
    xTaskNotify(vBMS_Polling_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits); // start poll
  }

  void onDisconnect(BLEClient *pclient)
  {
    log_d("disabling polling");
    bmsDevice = nullptr;
    vTaskSuspend(vBMS_Polling_hdl);
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
  xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_INIT_BIT, eSetBits);

  return true;
}

void writeValue(std::vector<uint8_t> val)
{
  static TickType_t xBmsLastRequest = xTaskGetTickCount();
  if (pTxRemoteCharacteristic == nullptr)
  {
    log_e("Remote TX characteristic unavailable");
    vTaskDelay(pdMS_TO_TICKS(1000));
    return;
  }
  // graceperiod for BMS to process
  vTaskDelayUntil(&xBmsLastRequest, pdMS_TO_TICKS(BMS_UPDATE_DELAY));
  pTxRemoteCharacteristic->writeValue(val.data(), val.size());
}

void vBasicInfoPollIntervaller(void *parameter)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (true)
  {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BMS_BASIC_INFO_DELAY));
    xTaskNotify(vBMS_Polling_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits); // start poll
  }
}

void vCellInfoPollIntervaller(void *parameter)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (true)
  {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BMS_CELL_INFO_DELAY));
    xTaskNotify(vBMS_Polling_hdl, NotificationBits::CELL_UPDATE_BIT, eSetBits); // start poll
  }
}

void vBMS_Polling(void *parameter)
{
  vTaskSuspend(NULL); // suspend self until connected
  xTaskCreatePinnedToCore(vBasicInfoPollIntervaller, "BASICPOLLER", 1000, NULL, 3, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(vCellInfoPollIntervaller, "CELLPOLLER", 1000, NULL, 2, NULL, tskNO_AFFINITY);

  while (true)
  {
    uint32_t ulNotifiedValue;
    // wait for notification trigger
    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY) == pdFAIL)
    {
      log_e("no notification for BMS request, timeout, restart");
      continue;
    }
    byte mask = 1;
    while (ulNotifiedValue)
    {
      switch (ulNotifiedValue & mask)
      {
      case NotificationBits::FET_ENABLE_BIT:
        log_i("FET enable received");
        writeValue(mosfetChargeCtrlRequest(true));
        break;

      case NotificationBits::FET_DISABLE_BIT:
        log_i("FET disable received");
        writeValue(mosfetChargeCtrlRequest(false));
        break;

      case NotificationBits::BMS_UPDATE_BIT:
        log_i("BMS update received");
        writeValue(basicRequest());
        break;

      case NotificationBits::CELL_UPDATE_BIT:
        log_i("BMS cellinfo received");
        writeValue(cellInfoRequest());
        break;

        // default:
        // log_e("unknown notification for BMS request %02X", ulNotifiedValue); // should not happen
      }
      ulNotifiedValue &= ~mask;
      mask <<= 1;
    }
  }
  vTaskDelete(NULL);
}

void beginBLE()
{
  BLEDevice::init(myName);
  log_d("vBMS_Scan: %d", xPortGetCoreID());
  pClient = BLEDevice::createClient();
  log_d(" - Created client");
  pClient->setClientCallbacks(new MyClientCallback());
}

void vBMS_Scan(void *parameter)
{
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
