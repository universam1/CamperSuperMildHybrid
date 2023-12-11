#ifndef _GLOBALS_H
#define _GLOBALS_H

#pragma once

#include <Arduino.h>
#include <bms2.h>

enum NotificationBits
{
    BMS_INIT_BIT = 1 << 0,
    BMS_UPDATE_BIT = 1 << 1,
    CELL_UPDATE_BIT = 1 << 2,
    OBD_INIT_BIT = 1 << 3,
    OBD_UPDATE_BIT = 1 << 4,
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
  uint16_t rpm;
  uint16_t speed;
  uint8_t load;
  bool coasting;
} OBDdata_t;

typedef struct BMSInfo_t
{
  float Voltage;
  float Current;
  float Power;
float CellVoltages[4];
} BMSInfo_t;

extern OBDdata_t obdData;

extern BMSInfo_t bmsInfo;

extern TaskHandle_t vBMS_Task_hdl, vOBD_Task_hdl, vTFT_Task_hdl;
void vBMS_Task(void *parameter);
void vOBD_Task(void *parameter);
void vTFT_Task(void *parameter);
#endif // _GLOBALS_H
