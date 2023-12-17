#include "smarthybrid.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *myDevice;
BLERemoteService *pRemoteService;
// The remote service we wish to connect to. Needs check/change when other BLE module used.
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); //xiaoxiang bms original module
// 0000ff01-0000-1000-8000-00805f9b34fb
// NOTIFY, READ
// Notifications from this characteristic is received data from BMS

// 0000ff02-0000-1000-8000-00805f9b34fb
// Write this characteristic to send data to BMS
// READ, WRITE, WRITE NO RESPONSE

bool doConnect = false;
bool BLE_client_connected = false;
bool doScan = false;
bool toggle = false;
bool newPacketReceived = false;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ //this is called by some underlying magic
    /**
	* Called for each advertising BLE server.
	*/
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        log_d("BLE Advertised Device found: ");
        log_d(advertisedDevice.toString().c_str());

        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
        {

            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;

        } // Found our server
    }     // onResult
};        // MyAdvertisedDeviceCallbacks
class MyClientCallback : public BLEClientCallbacks
{ //this is called on connect / disconnect by some underlying magic+

    void onConnect(BLEClient *pclient)
    {
        log_d("onConnect");

    }

    void onDisconnect(BLEClient *pclient)
    {
        BLE_client_connected = false;
        log_d("onDisconnect");
    }
};

void bleRequestData()
{
#ifndef SIMULATION

    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    if (doConnect == true)
    {
        if (connectToServer())
        {
            log_d("We are now connected to the BLE Server.");
        }
        else
        {
            // lcdConnectionFailed();
        }
        doConnect = false;
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (BLE_client_connected == true)
    {

        unsigned long currentMillis = millis();
        if ((currentMillis - previousMillis >= interval || newPacketReceived)) //every time period or when packet is received
        {
            previousMillis = currentMillis;

            if (toggle) //alternate info3 and info4
            {
                bmsGetInfo3();
                //showBasicInfo();
                newPacketReceived = false;
            }
            else
            {
                bmsGetInfo4();
                //showCellInfo();
                newPacketReceived = false;
            }
            toggle = !toggle;
        }
    }
    else if (doScan)
    {
        BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }
#endif

#ifdef SIMULATION
    bmsSimulate();
#endif
}

void bleStartup()
{
#ifndef SIMULATION

    BLEDevice::init("");

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, true);
#endif
}

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) //this is called when BLE server sents data via notofication
{
    //hexDump((char*)pData, length);
    toCircularBuffer(pData, length);
}

bool connectToServer()
{
    log_d("Forming a connection to ");
    log_d(myDevice->getAddress().toString().c_str());
    BLEClient *pClient = BLEDevice::createClient();
    log_d(" - Created client");
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    log_d(" - Connected to server");
    // Obtain a reference to the service we are after in the remote BLE server.
    //BLERemoteService*
    pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr)
    {
        log_d("Failed to find our service UUID: ");
        log_d(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    log_d(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
    if (pRemoteCharacteristic == nullptr)
    {
        log_d("Failed to find our characteristic UUID: ");
        log_d(charUUID_rx.toString().c_str());
        pClient->disconnect();
        return false;
    }
    log_d(" - Found our characteristic");
    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead())
    {
        std::string value = pRemoteCharacteristic->readValue();
        log_d("The characteristic value was: ");
        log_d(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

    BLE_client_connected = true;
}

void sendCommand(uint8_t *data, uint32_t dataLen)
{

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);

    if (pRemoteCharacteristic)
    {
        pRemoteCharacteristic->writeValue(data, dataLen);
        //log_d("bms request sent");
    }
    else
    {
        log_d("Remote TX characteristic not found");
    }
}