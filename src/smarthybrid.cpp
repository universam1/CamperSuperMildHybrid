#include <Arduino.h>
#include "BluetoothSerial.h"
#include "ELMduino.h"
#include "BleSerialClient.h"
#include "SSD1306Wire.h"
#include <bms2.h>

SSD1306Wire tft(0x3c, 5, 4);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

BluetoothSerial SerialBT;
// #define ELM_PORT SerialBT
#define DEBUG_PORT Serial

ELM327 myELM327;
typedef enum
{
  FIRST_PID,
  ENG_RPM = FIRST_PID,
  ENG_LOAD,
  // SPEED,
  LAST_PID
} obd_pid_states;

static char buffer[32];

BleSerialClient BLEClient;

void keycommands()
{
  if (Serial.available())
  {
    char c = Serial.read();
    if (c == 'd')
    {
      Serial.println("Disconnecting bluetooth connection");
      SerialBT.disconnect();
      // Serial.println("Closing bluetooth connection");
      // SerialBT.end();
      Serial.println("Halting program ... ");

#ifdef lcd_20x4_line_char
      lcd.clear();
      lcd.setCursor(0, 0);
      //         01234567890123456789
      lcd.print("Disconnect Bluetooth");
      lcd.setCursor(0, 1);
      //         01234567890123456789
      lcd.print("Halting program ...");
#endif

      while (1)
        ;
    }
    else if (c == 'P')
    {
      char cmd[100] = {0};
      // Print supported PIDS
      Serial.println("\n\nPrint supported PIDs *** Turn engine ON ***");
      Serial.println("Turning ECU filter off ATCRA");
      myELM327.sendCommand("ATCRA");
      Serial.println("Turning headers ON ATH1");
      myELM327.sendCommand("ATH1");
      Serial.println("Turning line feeds ON ATL1");
      myELM327.sendCommand("ATL1");
      Serial.println("Turning spaces ON ATS1\n");
      myELM327.sendCommand("ATS1");

      Serial.println("=================================");
      for (int i = 0; i < 6; i++)
      {
        uint8_t pid = i * 32;
        Serial.printf("Supported PIDs: 01%02X\n", pid);
        sprintf(cmd, "01%02X", pid);
        myELM327.sendCommand(cmd);
        Serial.printf("%s", myELM327.payload);
        delay(100);
      }
      Serial.println("=================================\n");
      while (1)
        ;
    }
  }
}

// #define USE_NAME          // Comment this to use MAC address instead of a slaveName
const char *pin = "1234"; // Change this to reflect the pin expected by the real slave BT device

// Name: OBDII, Address: 66:1e:21:00:aa:fe, cod: 23603, rssi: -44
OverkillSolarBms2 bms = OverkillSolarBms2();
#define m_last_basic_info_query_rate 250
#define m_last_cell_voltages_query_rate 1000
BasicInfo bmsInfo;
float CellVoltages[4];
TaskHandle_t vBMS_Task_hdl, vOBD_Task_hdl, vTFT_Task_hdl;

enum NotificationBits
{
  BMS_INIT_BIT = 1 << 0,
  BMS_UPDATE_BIT = 1 << 1,
  CELL_UPDATE_BIT = 1 << 2,
  OBD_INIT_BIT = 1 << 3,
  OBD_UPDATE_BIT = 1 << 4,
};

#ifdef USE_NAME
String slaveName = "OBDII"; // Change this to reflect the real name of your slave BT device
#else
String MACadd = "66:1e:21:00:aa:fe";                       // This only for printing
uint8_t address[6] = {0x66, 0x1e, 0x21, 0x00, 0xaa, 0xfe}; // Change this to reflect real MAC address of your slave BT device
#endif

String myName = "ESP32-BT-Master";

typedef struct OBDdata_t
{
  uint16_t rpm;
  uint16_t speed;
  uint8_t load;
  bool coasting;
} OBDdata_t;
OBDdata_t obdData;

void vOBD_Task(void *parameter)
{
  static int state;

  if (!SerialBT.begin(myName, true))
  {
    Serial.println("An error occurred initializing Bluetooth");
    sleep(1000);
    ESP.restart();
  }

  if (!SerialBT.setPin(pin))
  {
    Serial.println("An error occurred setting the PIN");
    sleep(1000);
    ESP.restart();
  }
  bool connected;
// connect(address) is fast (up to 10 secs max), connect(slaveName) is slow (up to 30 secs max) as it needs
// to resolve slaveName to address first, but it allows to connect to different devices with the same name.
// Set CoreDebugLevel to Info to view devices Bluetooth address and device names
#ifdef USE_NAME
  connected = SerialBT.connect(slaveName);
  Serial.printf("Connecting to slave BT device named \"%s\"\n", slaveName.c_str());
#else
  connected = SerialBT.connect(address, 0, ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER);
  Serial.print("Connecting to slave BT device with MAC ");
  Serial.println(MACadd);
#endif

  if (connected)
  {
    Serial.println("Connected Successfully!");
  }
  else
  {
    while (!SerialBT.connected(10000))
    {
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app.");
      ESP.restart();
    }
  }

  if (!myELM327.begin(SerialBT, true, 2000))
  {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    ESP.restart();
  }
  xTaskNotify(vTFT_Task_hdl, NotificationBits::OBD_INIT_BIT, eSetBits);
  while (true)
  {
    state = state % LAST_PID;
    switch (state)
    {
    case ENG_RPM:
    {
      auto val = myELM327.rpm();
      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        obdData.rpm = val;
        state++;
      }
      break;
    }
    case ENG_LOAD:
    {
      auto val = myELM327.engineLoad();
      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        obdData.load = val;
        obdData.coasting = (obdData.rpm > 0 && obdData.load == 0);
        xTaskNotify(vTFT_Task_hdl, NotificationBits::OBD_UPDATE_BIT, eSetBits);
        state++;
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

    if (myELM327.nb_rx_state != ELM_SUCCESS && myELM327.nb_rx_state != ELM_GETTING_MSG)
    {
      myELM327.printError();
      state++;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void vBMS_Task(void *parameter)
{
  static uint32_t
      m_last_basic_info_query_time = 0,
      m_last_cell_voltages_query_time = 0;
  BLEClient.begin(myName.c_str());

  bms.begin(&BLEClient);

  while (true)
  {
    if (BLEClient.connected())
    {
      log_d("BLEClient connected");
      BLEClient.bleLoop();
      bms.main_task(false);
      uint32_t now = millis();

      if (millis() - m_last_basic_info_query_time >= m_last_basic_info_query_rate)
      {
        bms.query_0x03_basic_info();
        BasicInfo bmsInfo = bms.get_BasicInfo();
        log_i("BMS Info: %.2fV %.2fA %.2fW",
              bmsInfo.voltage / 1000.0, bmsInfo.current / 1000.0, bmsInfo.voltage / 1000.0 * bmsInfo.current / 1000.0);
        xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits);
        m_last_basic_info_query_time = millis();
      }

      if (millis() - m_last_cell_voltages_query_time >= m_last_cell_voltages_query_rate)
      {
        bms.query_0x04_cell_voltages();
        for (size_t i = 0; i < 4; i++)
          CellVoltages[i] = bms.get_cell_voltage(i);
        xTaskNotify(vTFT_Task_hdl, NotificationBits::CELL_UPDATE_BIT, eSetBits);
        m_last_cell_voltages_query_time = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
      log_e("BLEClient not connected");
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}
const TickType_t xMaxBlockTime = pdMS_TO_TICKS(2000);

void vTFT_Task(void *parameter)
{
  BaseType_t xResult;
  tft.init();
  tft.flipScreenVertically();
  tft.setFont(ArialMT_Plain_10);
  tft.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Booting");
  tft.display();
  uint32_t ulNotifiedValue;
  char buffer[32];

  if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue, xMaxBlockTime) == pdPASS)
  {
    if (ulNotifiedValue & NotificationBits::BMS_INIT_BIT)
    {
      log_i("BMS init received");
      tft.clear();
      tft.setFont(ArialMT_Plain_24);
      tft.drawString(32, 0, "BMS Connected");
      tft.display();
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (ulNotifiedValue & NotificationBits::OBD_INIT_BIT)
    {
      log_i("OBD init received");
      tft.clear();
      tft.setFont(ArialMT_Plain_24);
      tft.drawString(32, 0, "ELM327 ok");
      tft.display();
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (ulNotifiedValue & NotificationBits::BMS_UPDATE_BIT)
    {
      log_i("BMS update received");
      tft.clear();
      tft.setFont(ArialMT_Plain_10);
      tft.drawStringf(0, 16, buffer, "%2.1fV", (bmsInfo.voltage / 1000.0));
      tft.drawStringf(0, 32, buffer, "%4.*fA", (abs(bmsInfo.current) < 10000 ? 1 : 0), bmsInfo.current / 1000.0);
      float p = bmsInfo.voltage / 1000.0 * bmsInfo.current / 1000.0;
      tft.drawStringf(0, 48, buffer, "%4.*fW", (abs(p) < 10.0 ? 1 : 0), p);
      tft.display();
    }
    if (ulNotifiedValue & NotificationBits::CELL_UPDATE_BIT)
    {
      log_i("Cell update received");
      float minC = CellVoltages[0], maxC = CellVoltages[0];
      for (size_t i = 1; i < 4; i++)
      {
        minC = min(minC, CellVoltages[i]);
        maxC = max(maxC, CellVoltages[i]);
      }

      tft.clear();
      tft.setFont(ArialMT_Plain_16);
      tft.drawStringf(0, 16, buffer, "Cell Δ: %.0fmV", (maxC - minC) * 1000.0);
      tft.display();
    }

    if (ulNotifiedValue & NotificationBits::OBD_UPDATE_BIT)
    {
      log_i("OBD update received");
      obdData.coasting ? tft.invertDisplay() : tft.normalDisplay();
      tft.clear();
      tft.setFont(ArialMT_Plain_16);
      tft.drawString(0, 16, "RPM: " + String(obdData.rpm));
      tft.drawString(0, 32, "Speed: " + String(obdData.speed));
      tft.drawString(0, 48, "Load: " + String(obdData.load));
      tft.drawString(0, 64, "Coasting: " + String(obdData.coasting));
      tft.display();
    }
  }
  else
  {
    log_e("Did not receive a notification within the expected time.");
    tft.clear();
    tft.setFont(ArialMT_Plain_16);
    tft.drawString(32, 0, "Offline");
    tft.display();
  }
}
void setup()
{

  DEBUG_PORT.begin(115200);
  Serial.println("booting");

  xTaskCreatePinnedToCore(
      vTFT_Task,       /* Function to implement the task */
      "TFT",           /* Name of the task */
      1000,            /* Stack size in words */
      NULL,            /* Task input parameter */
      10,              /* Priority of the task */
      &vTFT_Task_hdl,  /* Task handle. */
      tskNO_AFFINITY); /* Core where the task should run */

  xTaskCreatePinnedToCore(
      vBMS_Task,       /* Function to implement the task */
      "BMS",           /* Name of the task */
      1000,            /* Stack size in words */
      NULL,            /* Task input parameter */
      5,               /* Priority of the task */
      &vBMS_Task_hdl,  /* Task handle. */
      tskNO_AFFINITY); /* Core where the task should run */

  xTaskCreatePinnedToCore(
      vOBD_Task,       /* Function to implement the task */
      "OBD",           /* Name of the task */
      1000,            /* Stack size in words */
      NULL,            /* Task input parameter */
      3,               /* Priority of the task */
      &vOBD_Task_hdl,  /* Task handle. */
      tskNO_AFFINITY); /* Core where the task should run */

  // // Disconnect() may take up to 10 secs max
  // if (SerialBT.disconnect())
  // {
  //   Serial.println("Disconnected Successfully!");
  // }
  // // This would reconnect to the slaveName(will use address, if resolved) or address used with connect(slaveName/address).
  // SerialBT.connect();
  // if (connected)
  // {
  //   Serial.println("Reconnected Successfully!");
  // }
  // else
  // {
  //   while (!SerialBT.connected(10000))
  //   {
  //     Serial.println("Failed to reconnect. Make sure remote device is available and in range, then restart app.");
  //   }
  // }

  // ELM_PORT.setPin("1234");
  // Serial.println("The device started, now you can pair it with bluetooth!");

  // if (btScanAsync)
  // {
  //   Serial.print("Starting discoverAsync...");
  //   if (SerialBT.discoverAsync(btAdvertisedDeviceFound))
  //   {
  //     Serial.println("Findings will be reported in \"btAdvertisedDeviceFound\"");
  //     delay(10000);
  //     Serial.print("Stopping discoverAsync... ");
  //     SerialBT.discoverAsyncStop();
  //     Serial.println("stopped");
  //   }
  //   else
  //   {
  //     Serial.println("Error on discoverAsync f.e. not workin after a \"connect\"");
  //   }
  // }

  // if (btScanSync)
  // {
  //   Serial.println("Starting discover...");
  //   BTScanResults *pResults = SerialBT.discover(BT_DISCOVER_TIME);
  //   if (pResults)
  //     pResults->dump(&Serial);
  //   else
  //     Serial.println("Error on BT Scan, no result!");
  // }

  // if (!ELM_PORT.begin("T5", true))
  // {
  //   DEBUG_PORT.println("Couldn't beging Bluetooth");
  //   tft.clear();
  //   tft.drawStringMaxWidth(0, 0, 128, "Couldn't begingBluetooth");
  //   tft.display();
  //   sleep(1000);
  //   ESP.restart();
  // }

  // if (!ELM_PORT.setPin("1234"))
  // {
  //   DEBUG_PORT.println("Couldn't set pin");
  //   tft.clear();
  //   tft.drawStringMaxWidth(0, 0, 128, "Couldn't set pin");
  //   tft.display();
  //   sleep(1000);
  //   ESP.restart();
  // };

  // if (!ELM_PORT.connect(MACadd))
  // {
  //   DEBUG_PORT.println("Couldn't connect to OBD via Bluetooth");
  //   tft.clear();
  //   tft.drawStringMaxWidth(0, 0, 128, "Couldn't connect to OBD via Bluetooth");
  //   tft.display();
  //   sleep(1000);
  //   ESP.restart();
  // }
}

void loop()
{
  keycommands();
}
