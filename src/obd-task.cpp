#include "smarthybrid.h"

#include "BluetoothSerial.h"
#include "ELMduino.h"

TaskHandle_t vOBD_Task_hdl;

// #define SIMULATE_OBD

BluetoothSerial SerialBT;

ELM327 myELM327;

// #define USE_NAME          // Comment this to use MAC address instead of a slaveName
const char *pin = "1234"; // Change this to reflect the pin expected by the real slave BT device

// Name: OBDII, Address: 66:1e:21:00:aa:fe, cod: 23603, rssi: -44

#ifdef USE_NAME
String slaveName = "OBDII"; // Change this to reflect the real name of your slave BT device
#else
String MACadd = "66:1e:21:00:aa:fe";                       // This only for printing
uint8_t address[6] = {0x66, 0x1e, 0x21, 0x00, 0xaa, 0xfe}; // Change this to reflect real MAC address of your slave BT device
#endif

OBDdata_t obdData;

// must be in strict order with BLE initialization
void beginSerialBT()
{
  if (!SerialBT.begin(MYNAME, true))
  {
    Serial.println("An error occurred initializing Bluetooth");
    delay(1000);
    ESP.restart();
  }
  log_d("sBT begin");
  if (!SerialBT.setPin(pin))
  {
    Serial.println("An error occurred setting the PIN");
    delay(1000);
    ESP.restart();
  }
}

void vOBD_Task(void *parameter)
{
  while (true)
  {

    log_d("vOBD_Task: %d", xPortGetCoreID());

    static int state;

#ifndef SIMULATE_OBD

    // connect(address) is fast (up to 10 secs max), connect(slaveName) is slow (up to 30 secs max) as it needs
    // to resolve slaveName to address first, but it allows to connect to different devices with the same name.
    // Set CoreDebugLevel to Info to view devices Bluetooth address and device names

    log_i("Connecting to slave BT device named \"%s\"\n", slaveName.c_str());
    if (SerialBT.connect(address, 0, ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER))
    {
      log_i("Connected Successfully!");
    }
    else
    {
      if (!SerialBT.connected(10000))
      {
        log_e("Failed to connect OBD. Make sure remote device is available");
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
    }

    if (!myELM327.begin(SerialBT, ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG, 2000))
    {
      log_e("Couldn't connect to OBD scanner - Phase 2");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
#endif

    xTaskNotify(vTFT_Task_hdl, NotificationBits::OBD_INIT_BIT, eSetBits);
    while (true)
    {
      if (!myELM327.connected)
      {
        log_e("Not connected to OBD");
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }

      state = state % LAST_PID;
      log_d("state: %d", state);
      switch (state)
      {
      case ENG_RPM:
      {
#ifndef SIMULATE_OBD
        auto val = myELM327.rpm();
        if (myELM327.nb_rx_state == ELM_SUCCESS)
#else
        auto val = random(820, 1360);
#endif
        {
          obdData.RPM = val;
          state++;
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      }
      case ENG_LOAD:
      {
#ifndef SIMULATE_OBD
        auto val = myELM327.engineLoad();
        if (myELM327.nb_rx_state == ELM_SUCCESS)
#else
        auto v = random(-50, 100);
        auto val = (v < 0) ? 0 : v;
#endif
        {
          obdData.Load = val;
          log_i("RPM: %d   Load: %d", obdData.RPM, obdData.Load);
          obdData.xLastUpdateTime = xTaskGetTickCount();
          xTaskNotify(vTFT_Task_hdl, NotificationBits::OBD_UPDATE_BIT, eSetBits);
          xTaskNotify(vMngCoasting_Task_hdl, NotificationBits::OBD_UPDATE_BIT, eSetBits);
          state++;
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      }

        // case SPEED:
        // {
        //   kph = myELM327.kph();
        //   if (myELM327.nb_rx_state == ELM_SUCCESS)
        //   {
        //     Serial.printf("km/h: %.0f", kph);
        //     tft.drawStringf(0, 15, buffer, "km/h: %.0f", kph);
        //     state++;
        //   }
        //   break;
        // }
      }
#ifndef SIMULATE_OBD
      if (myELM327.nb_rx_state != ELM_SUCCESS && myELM327.nb_rx_state != ELM_GETTING_MSG)
      {
        myELM327.printError();
        state++;
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      vTaskDelay(pdMS_TO_TICKS(2));
#else
      vTaskDelay(pdMS_TO_TICKS(500));
#endif
    }
  }
  vTaskDelete(NULL);
}
