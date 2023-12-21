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
uint32_t bmsUpdateCouter = 0;
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

void manageCharging()
{
  static uint32_t lastCoasting = 0;
  static uint32_t lastBmsUpdateCouter = 0;

  if (obdData.coasting)
    lastCoasting = millis();

  // prevent race condition with bms update
  if (lastBmsUpdateCouter == bmsUpdateCouter)
  {
    log_i("No BMS update since last OBD update");
    return;
  }

  // stop charging if load is above 50% or 20% for more than 10 seconds
  if (bmsInfo.chargeFET)
  {
    // hysterese
    if (obdData.load > 20 && millis() - lastCoasting > 1000)
    {
      log_i("Disabling charging delayed");
      xTaskNotify(vBMS_Polling_hdl, NotificationBits::FET_DISABLE_BIT, eSetBits);
      lastBmsUpdateCouter = bmsUpdateCouter;
      tft.invertDisplay(false);
    }
    else if (obdData.load > 50)
    {
      log_i("Disabling charging forced");
      xTaskNotify(vBMS_Polling_hdl, NotificationBits::FET_DISABLE_BIT, eSetBits);
      lastBmsUpdateCouter = bmsUpdateCouter;
      tft.invertDisplay(false);
    }
  }
  else
  // start charging if load is below 20% and charging is not enabled
  {
    if (obdData.coasting)
    {
      log_i("Enabling charging");
      xTaskNotify(vBMS_Polling_hdl, NotificationBits::FET_ENABLE_BIT, eSetBits);
      lastBmsUpdateCouter = bmsUpdateCouter;
      tft.invertDisplay(true);
    }
  }
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
        bmsUpdateCouter++;
        tft.setTextSize(2);
        tft.setCursor(70, 0);
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
        manageCharging();

        drawProgressbar(0, 0, 60, 12, obdData.load);
        tft.setCursor(0, 15);
        tft.printf("%4dUmin", obdData.rpm);

        tft.setCursor(0, 25);
        tft.printf("%3d%%", obdData.load);
        tft.display();
      }
    }
    else
    {
      log_e("Did not receive a notification within the expected time.");
      tft.clearDisplay();
      tft.setCursor(0, SCREEN_HEIGHT / 2);
      tft.print("Offline");
      tft.display();
    }
  }
  vTaskDelete(NULL);
}
