#include "smarthybrid.h"

// static char buffer[32];

// void keycommands()
// {
//   if (Serial.available())
//   {
//     char c = Serial.read();
//     if (c == 'd')
//     {
//       Serial.println("Disconnecting bluetooth connection");
//       SerialBT.disconnect();
//       // Serial.println("Closing bluetooth connection");
//       // SerialBT.end();
//       Serial.println("Halting program ... ");

// #ifdef lcd_20x4_line_char
//       lcd.clear();
//       lcd.setCursor(0, 0);
//       //         01234567890123456789
//       lcd.print("Disconnect Bluetooth");
//       lcd.setCursor(0, 1);
//       //         01234567890123456789
//       lcd.print("Halting program ...");
// #endif

//       while (1)
//         ;
//     }
//     else if (c == 'P')
//     {
//       char cmd[100] = {0};
//       // Print supported PIDS
//       Serial.println("\n\nPrint supported PIDs *** Turn engine ON ***");
//       Serial.println("Turning ECU filter off ATCRA");
//       myELM327.sendCommand("ATCRA");
//       Serial.println("Turning headers ON ATH1");
//       myELM327.sendCommand("ATH1");
//       Serial.println("Turning line feeds ON ATL1");
//       myELM327.sendCommand("ATL1");
//       Serial.println("Turning spaces ON ATS1\n");
//       myELM327.sendCommand("ATS1");

//       Serial.println("=================================");
//       for (int i = 0; i < 6; i++)
//       {
//         uint8_t pid = i * 32;
//         Serial.printf("Supported PIDs: 01%02X\n", pid);
//         sprintf(cmd, "01%02X", pid);
//         myELM327.sendCommand(cmd);
//         Serial.printf("%s", myELM327.payload);
//         delay(100);
//       }
//       Serial.println("=================================\n");
//       while (1)
//         ;
//     }
//   }
// }

void setup()
{

  Serial.begin(115200);
  Serial.printf("booting %d", __COUNTER__);

  xTaskCreatePinnedToCore(vTFT_Task, "TFT", 5000, NULL, 6, &vTFT_Task_hdl, 1);
  BMSStart();
  xTaskCreatePinnedToCore(vOBD_Task, "OBD", 5000, NULL, 4, &vOBD_Task_hdl, 0);

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
  vTaskDelete(NULL);
}

void loop()
{
  vTaskDelete(NULL);
}
