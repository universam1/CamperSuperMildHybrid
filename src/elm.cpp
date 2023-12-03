#include <Arduino.h>
#include "BluetoothSerial.h"
#include "ELMduino.h"
#include "SSD1306Wire.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

SSD1306Wire tft(0x3c, 5, 4);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
#define DEBUG_PORT Serial

ELM327 myELM327;
typedef enum
{
  FIRST,
  ENG_RPM = FIRST,
  SPEED,
  ENG_LOAD,
  TORQUE,
  LAST
} obd_pid_states;

float rpm, kph, load;
static char buffer[32];

void print_binary(uint32_t number)
{
  if (number >> 1)
  {
    print_binary(number >> 1);
  }
  Serial.print((number & 1) ? '1' : '0');
}

/*
-----------------
  Detect if user has pressed 'd' key on serial monitor keyboard, if yes
  End the bluetooth connection and halt the processor
  If 'P' is pressed, outputs the PIDs that the vehicle supports.
-----------------
*/
void quit_prog_if_keypressed()
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

static bool btScanAsync = true;
static bool btScanSync = true;

#define BT_DISCOVER_TIME 10000

void btAdvertisedDeviceFound(BTAdvertisedDevice *pDevice)
{
  Serial.printf("Found a device asynchronously: %s\n", pDevice->toString().c_str());
}

// #define USE_NAME          // Comment this to use MAC address instead of a slaveName
const char *pin = "1234"; // Change this to reflect the pin expected by the real slave BT device

// Name: OBDII, Address: 66:1e:21:00:aa:fe, cod: 23603, rssi: -44

#ifdef USE_NAME
String slaveName = "OBDII"; // Change this to reflect the real name of your slave BT device
#else
String MACadd = "66:1e:21:00:aa:fe";                       // This only for printing
uint8_t address[6] = {0x66, 0x1e, 0x21, 0x00, 0xaa, 0xfe}; // Change this to reflect real MAC address of your slave BT device
#endif

String myName = "ESP32-BT-Master";

void setup()
{

  tft.init();
  tft.flipScreenVertically();
  tft.setFont(ArialMT_Plain_10);
  tft.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, "Booting");
  tft.display();
  DEBUG_PORT.begin(115200);
  Serial.println("booting");

  SerialBT.begin(myName, true);
  Serial.printf("The device \"%s\" started in master mode, make sure slave BT device is on!\n", myName.c_str());

#ifndef USE_NAME
  SerialBT.setPin(pin);
  Serial.println("Using PIN");
#endif
  bool connected;
// connect(address) is fast (up to 10 secs max), connect(slaveName) is slow (up to 30 secs max) as it needs
// to resolve slaveName to address first, but it allows to connect to different devices with the same name.
// Set CoreDebugLevel to Info to view devices Bluetooth address and device names
#ifdef USE_NAME
  connected = SerialBT.connect(slaveName);
  Serial.printf("Connecting to slave BT device named \"%s\"\n", slaveName.c_str());
#else
  connected = SerialBT.connect(address);
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
    }
  }
  // Disconnect() may take up to 10 secs max
  if (SerialBT.disconnect())
  {
    Serial.println("Disconnected Successfully!");
  }
  // This would reconnect to the slaveName(will use address, if resolved) or address used with connect(slaveName/address).
  SerialBT.connect();
  if (connected)
  {
    Serial.println("Reconnected Successfully!");
  }
  else
  {
    while (!SerialBT.connected(10000))
    {
      Serial.println("Failed to reconnect. Make sure remote device is available and in range, then restart app.");
    }
  }

  ELM_PORT.setPin("1234");
  Serial.println("The device started, now you can pair it with bluetooth!");

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

  if (!myELM327.begin(ELM_PORT, true, 2000))
  {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    tft.clear();
    tft.drawStringMaxWidth(0, 0, 128, "Couldn't connect to OBD scanner - Phase 2");
    tft.display();
    sleep(1000);
    ESP.restart();
  }

  Serial.println("Connected to ELM327");
  tft.clear();
  tft.drawString(0, 0, "Connected to ELM327");
}

void loop()
{
  static int state;
  state = state % LAST;

  switch (state)
  {
  case ENG_RPM:
  {
    rpm = myELM327.rpm();
    if (myELM327.nb_rx_state == ELM_SUCCESS)
    {
      Serial.printf("rpm: %.0f", rpm);
      tft.drawStringf(0, 0, buffer, "U/min: %.0f", rpm);
      state++;
    }
    break;
  }

  case SPEED:
  {
    kph = myELM327.kph();
    if (myELM327.nb_rx_state == ELM_SUCCESS)
    {
      Serial.printf("km/h: %.0f", kph);
      tft.drawStringf(0, 15, buffer, "km/h: %.0f", kph);
      state++;
    }
    break;
  }
  case ENG_LOAD:
  {
    load = myELM327.engineLoad();
    if (myELM327.nb_rx_state == ELM_SUCCESS)
    {
      Serial.printf("load: %f", load);
      tft.drawStringf(0, 30, buffer, "load: %.0f", load);
      state++;
    }
    break;
  }
  case TORQUE:
  {
    auto v = myELM327.torque();
    if (myELM327.nb_rx_state == ELM_SUCCESS)
    {
      Serial.printf("torque: %f", v);
      tft.drawStringf(0, 45, buffer, "torque: %.0f", v);
      state++;
    }
    break;
  }
  }

  // if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  // {
  //   myELM327.printError();
  //   tft.clear();
  //   tft.drawStringf(0, 32, buffer, "ERROR: %d", myELM327.nb_rx_state);
  //   state++;
  // }
  tft.display();
  quit_prog_if_keypressed();
  // sleep(10);
}
