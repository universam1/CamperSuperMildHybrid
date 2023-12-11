#include "smarthybrid.h"
#include "BleSerialClient.h"
#define SIMULATE_BMS

BleSerialClient BLEClient;

TaskHandle_t vBMS_Task_hdl;
BMSInfo_t bmsInfo;

OverkillSolarBms2 bms = OverkillSolarBms2();
#define basic_info_query_rate 250
#define cell_query_rate 3000
void vBMS_Task(void *parameter)
{
  log_d("vBMS_Task: %d", xPortGetCoreID());

  static uint32_t
      m_last_basic_info_query_time = 0,
      m_last_cell_voltages_query_time = 0;
#ifndef SIMULATE_BMS
  BLEClient.begin(myName.c_str());
  bms.begin(&BLEClient);
#endif
  xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_INIT_BIT, eSetBits);

  while (true)
  {
#ifndef SIMULATE_BMS
    if (BLEClient.connected())
    {
      log_d("BLEClient connected");
      BLEClient.bleLoop();
      bms.main_task(false);
#else
    {
#endif
      if (millis() - m_last_basic_info_query_time >= basic_info_query_rate)
      {
#ifndef SIMULATE_BMS
        bms.query_0x03_basic_info();
        bmsInfo.voltage = bms.get_voltage();
        bmsInfo.current = bms.get_current();
#else
        bmsInfo.Voltage = random(12800, 14600) / 1000.0;
        bmsInfo.Current = random(-1800, 600) / 1000.0;
#endif
        bmsInfo.Power = bmsInfo.Voltage * bmsInfo.Current;
        log_d("BMS Info: %.2fV %.2fA %.2fW",
              bmsInfo.Voltage, bmsInfo.Current, bmsInfo.Power);
        xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits);
        m_last_basic_info_query_time = millis();
      }

      if (millis() - m_last_cell_voltages_query_time >= cell_query_rate)
      {
#ifndef SIMULATE_BMS
        bms.query_0x04_cell_voltages();
#endif
        for (size_t i = 0; i < 4; i++)
#ifndef SIMULATE_BMS
          CellVoltages[i] = bms.get_cell_voltage(i);
#else
          bmsInfo.CellVoltages[i] = random(3100, 3300) / 1000.0;
#endif
        xTaskNotify(vTFT_Task_hdl, NotificationBits::CELL_UPDATE_BIT, eSetBits);
        m_last_cell_voltages_query_time = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }
#ifndef SIMULATE_BMS
    else
    {
      log_e("BLEClient not connected");
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
#endif
  }
  vTaskDelete(NULL);
}
