#include "smarthybrid.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold12pt7b.h>
TaskHandle_t vTFT_Task_hdl;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // SSD1306 doesn't have a reset pin
#define SCREEN_ADDRESS 0x3c
#define OLED_SDA 5
#define OLED_SCL 4

Adafruit_SSD1306 tft(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
void drawProgressbar(int x, int y, int width, int height, int progress)
{
  progress = constrain(progress, 0, 100);
  float bar = ((float)(width - 1) / 100) * progress;
  tft.drawRect(x, y, width, height, WHITE);
  tft.fillRect(x + 2, y + 2, width - 4, height - 4, BLACK);
  tft.fillRect(x + 2, y + 2, bar, height - 4, WHITE);
}

String formatPower(float power, uint8_t precision)
{
  String p = String(power, 3);
  p = p.substring(0, precision);
  if (p.endsWith("."))
    p.remove(p.length() - 1);

  while (p.length() < precision)
    p = " " + p;
  return p;
}


void vTFT_Task(void *parameter)
{
  log_d("vTFT_Task: %d", xPortGetCoreID());
  BaseType_t xResult;
  // char buffer[16];
  Wire.begin(5, 4);
  // tft.init();
  if (!tft.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
  }
  tft.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  tft.setTextWrap(false);
  // tft.cp437(true);
  tft.clearDisplay();
  tft.setTextSize(2);

  tft.printf("Boot: %d", __COUNTER__);
  tft.display();
  uint32_t ulNotifiedValue;
  while (true)
  {
    tft.setTextSize(1);
    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue, pdMS_TO_TICKS(2000)) == pdPASS)
    {
      if (ulNotifiedValue & NotificationBits::BMS_INIT_BIT)
      {
        log_d("BMS init received");
        tft.clearDisplay();
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.print("BMS ok");
        tft.display();
        vTaskDelay(pdMS_TO_TICKS(600));
        tft.setTextSize(1);
        tft.clearDisplay();
      }

      if (ulNotifiedValue & NotificationBits::OBD_INIT_BIT)
      {
        log_d("OBD init received");
        tft.clearDisplay();
        tft.setTextSize(2);
        tft.setCursor(0, 30);
        tft.print("ELM327 ok");
        tft.display();
        vTaskDelay(pdMS_TO_TICKS(600));
        tft.setTextSize(1);
        tft.clearDisplay();
      }

      if (ulNotifiedValue & NotificationBits::BMS_UPDATE_BIT)
      {
        log_d("BMS update received");
        tft.invertDisplay(bmsInfo.chargeFET); // invert display if charging
        tft.setTextSize(2);
        tft.setCursor(58, 0);
        tft.printf("%2.2fV", (bmsInfo.Voltage));

        tft.setCursor(70, 20);
        tft.print(formatPower(bmsInfo.Current, 4));
        tft.setCursor(118, 20);
        tft.print("A");
        tft.setCursor(118, 50);
        tft.print("W");
        tft.setTextSize(3);
        tft.setCursor(46, 43);
        log_d("Power: %f : %s", bmsInfo.Power, formatPower(bmsInfo.Power, 5).c_str());
        tft.print(formatPower(bmsInfo.Power, 4));
        tft.display();
        tft.setTextSize(1);
      }
      if (ulNotifiedValue & NotificationBits::CELL_UPDATE_BIT)
      {
        log_d("Cell update received");
        tft.setCursor(0, 35);
        tft.printf("^%3dmV", bmsInfo.CellDiff);
        tft.display();
      }

      if (ulNotifiedValue & NotificationBits::OBD_UPDATE_BIT)
      {
        log_i("OBD update received");

        drawProgressbar(0, 0, 50, 12, obdData.Load);
        tft.setCursor(0, 15);
        tft.printf("%4dUmin", obdData.RPM);

        tft.setCursor(0, 25);
        tft.printf("%3d%%", obdData.Load);
        tft.display();
      }
    }
    else
    {
      log_e("Did not receive a notification within the expected time.");
      tft.clearDisplay();
      tft.invertDisplay(false);
      tft.setCursor(0, SCREEN_HEIGHT / 2);
      tft.setTextSize(2);
      tft.print("Offline");
      tft.display();
    }
  }
  vTaskDelete(NULL);
}
