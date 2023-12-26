#ifndef _GLOBALS_H
#define _GLOBALS_H

#pragma once

#include <Arduino.h>
#include <BLEDevice.h>

#define myName "SmartHybrid"

#define BMS_UPDATE_DELAY 50 // milliseconds
#define BMS_BASIC_INFO_DELAY 250
#define BMS_CELL_INFO_DELAY 1000

enum NotificationBits
{
  BMS_INIT_BIT = 1 << 0,
  BMS_UPDATE_BIT = 1 << 1,
  CELL_UPDATE_BIT = 1 << 2,
  OBD_INIT_BIT = 1 << 3,
  OBD_UPDATE_BIT = 1 << 4,
  FET_ENABLE_BIT = 1 << 5,
  FET_DISABLE_BIT = 1 << 6,
};

typedef enum
{
  FIRST_PID,
  ENG_RPM = FIRST_PID,
  ENG_LOAD,
  // SPEED,
  LAST_PID
} obd_pid_states;

typedef struct OBDdata_t
{
  TickType_t xLastUpdateTime;
  uint16_t RPM;
  uint8_t Load;
} OBDdata_t;

typedef struct BMSInfo_t
{
  float Voltage;
  float Current;
  float Power;
  float NTC[2];
  float CellVoltages[4];
  TickType_t xLastUpdateTime;
  uint16_t CellDiff;
  uint8_t stateFET;
  bool dischargeFET;
  bool chargeFET;
} BMSInfo_t;

extern OBDdata_t obdData;
extern BMSInfo_t bmsInfo;
extern TickType_t xBmsLastMsg;

void BMSStart();
void intoCircularBuffer(uint8_t *pData, size_t length);
void vBMSProcessTask(void *parameter);
std::vector<uint8_t> basicRequest();
std::vector<uint8_t> cellInfoRequest();
std::vector<uint8_t> mosfetChargeCtrlRequest(bool charge);

extern TaskHandle_t vOBD_Task_hdl,
    vTFT_Task_hdl,
    vBMSProcess_Task_hdl,
    vBMS_Polling_hdl,
    vMngCoasting_Task_hdl;
void vOBD_Task(void *parameter);
void vTFT_Task(void *parameter);
#endif // _GLOBALS_H
