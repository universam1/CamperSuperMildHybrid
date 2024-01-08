#include "smarthybrid.h"

TaskHandle_t vMngCoasting_Task_hdl;

void vMngCoasting_Task(void *parameter)
{
  TickType_t xlastCoasting = 0;
  TickType_t xlastBmsUpdate = 0;

  while (true)
  {
    // wait for notification trigger
    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, NULL, portMAX_DELAY) == pdFAIL)
    {
      log_e("no notification for manage coasting request, timeout");
      continue;
    }

    // coasting detection
    bool coasting = false;
    if (obdData.RPM > 0 && obdData.Load <= 10)
    {
      coasting = true;
      xlastCoasting = bmsInfo.xLastUpdateTime;
    }

    // prevent race condition with bms update
    if (xlastBmsUpdate == bmsInfo.xLastUpdateTime)
    {
      log_d("No BMS update since last OBD update");
      continue;
    }

    // stop charging if load is above 50% or 20% for more than 10 seconds
    if (bmsInfo.chargeFET)
    {
      // hysterese
      if (obdData.Load > 20 && xTaskGetTickCount() - xlastCoasting > pdMS_TO_TICKS(5000))
        log_i("Disabling charging delayed");

      else if (obdData.Load > 60)
        log_i("Disabling charging forced");

      else
        continue;

      xTaskNotify(vBMS_Polling_hdl, NotificationBits::FET_DISABLE_BIT, eSetBits);
      xlastBmsUpdate = bmsInfo.xLastUpdateTime;
    }
    else
    {
      // start charging if load is below 20% and charging is not enabled
      if (coasting)
        log_i("Enabling charging");
      // motor shutdown, enable the charge FET
      else if (obdData.RPM == 0 && obdData.Load == 0)
        log_i("motor shutdown, re-enable the charge FET");
      else
        continue;

      xTaskNotify(vBMS_Polling_hdl, NotificationBits::FET_ENABLE_BIT, eSetBits);
      xlastBmsUpdate = bmsInfo.xLastUpdateTime;
    }
  }
}

void setup()
{

  Serial.begin(115200);
  Serial.printf("booting %d", __COUNTER__);

  xTaskCreatePinnedToCore(vTFT_Task, "TFT", 5000, NULL, 6, &vTFT_Task_hdl, 1);

  // strict order of initialization of BLE and BT
  beginBLE();
  // vTaskDelay(pdMS_TO_TICKS(1000));
  beginSerialBT();

  xTaskCreatePinnedToCore(vBMS_Scan, "SCAN", 5000, NULL, 1, &vBMS_Scan_hdl, 0);
  xTaskCreatePinnedToCore(vBMS_Polling, "POLL", 3000, NULL, 2, &vBMS_Polling_hdl, 1);
  xTaskCreatePinnedToCore(vBMSProcessTask, "BMSProcess", 5000, NULL, 5, &vBMSProcess_Task_hdl, 1);

  xTaskCreatePinnedToCore(vOBD_Task, "OBD", 5000, NULL, 1, &vOBD_Task_hdl, 1);
  xTaskCreatePinnedToCore(vMngCoasting_Task, "Coasting", 5000, NULL, 5, &vMngCoasting_Task_hdl, 1);

  vTaskDelete(NULL);
}

void loop()
{
  vTaskDelete(NULL);
}
