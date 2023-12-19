#include "smarthybrid.h"
#include <CircularBuffer.h>

// Constants
#define BMS_STARTBYTE 0xDD
#define BMS_STOPBYTE 0x77
#define BMS_READ 0xA5
#define BMS_WRITE 0x5A

#define BMS_PARAM_TIMEOUT 500 // milliseconds

// Command codes (registers)
#define BMS_REG_BASIC_SYSTEM_INFO 0x03
#define BMS_REG_CELL_VOLTAGES 0x04
#define BMS_REG_NAME 0x05
#define BMS_REG_CTL_MOSFET 0xE1

// State machine states
#define BMS_STATE_WAIT_FOR_START_BYTE 0x00
#define BMS_STATE_WAIT_FOR_STATUS_BYTE 0x01
#define BMS_STATE_WAIT_FOR_CMD_CODE 0x02
#define BMS_STATE_WAIT_FOR_LENGTH 0x03
#define BMS_STATE_WAIT_FOR_DATA 0x04
#define BMS_STATE_WAIT_FOR_CHECKSUM_MSB 0x05
#define BMS_STATE_WAIT_FOR_CHECKSUM_LSB 0x06
#define BMS_STATE_WAIT_FOR_STOP_BYTE 0x07
#define BMS_STATE_ERROR 0xFF

CircularBuffer<uint8_t, 120> rbuffer;
TaskHandle_t vBMSProcess_Task_hdl;

typedef struct
{
  byte start;
  byte type;
  byte status;
  byte dataLen;
} bmsPacketHeaderStruct;

void intoCircularBuffer(uint8_t *pData, size_t length)
{
  for (size_t i = 0; i < length; i++)
    rbuffer.push(pData[i]);
  if (pData[length - 1] == BMS_STOPBYTE)
    // we have a frame
    xTaskNotify(vBMSProcess_Task_hdl, 0, eNoAction);
}

void handle_rx_0x03()
{

  bmsInfo.Voltage = ((uint16_t)(rbuffer[0] << 8) | rbuffer[1]) * 0.01; // 0-1   Total voltage
  bmsInfo.Current = ((uint16_t)(rbuffer[2] << 8) | rbuffer[3]) * 0.01; // 2-3   Current
  bmsInfo.Power = bmsInfo.Voltage * bmsInfo.Current;
  // bmsInfo.balance_capacity = (uint16_t)(rbuffer[4] << 8) | (uint16_t)(rbuffer[5]);    // 4-5   Balance capacity
  // bmsInfo.rate_capacity = (uint16_t)(rbuffer[6] << 8) | (uint16_t)(rbuffer[7]);       // 6-7   Rate capacity
  // bmsInfo.cycle_count = (uint16_t)(rbuffer[8] << 8) | (uint16_t)(rbuffer[9]);         // 8-9   Cycle count
  // bmsInfo.production_date = (uint16_t)(rbuffer[10] << 8) | (uint16_t)(rbuffer[11]);   // 10-11 Production Date
  // bmsInfo.balance_status = (uint16_t)(rbuffer[12] << 8) | (uint16_t)(rbuffer[13]);    // 12-13, 14-15 Balance Status
  // bmsInfo.protection_status = (uint16_t)(rbuffer[16] << 8) | (uint16_t)(rbuffer[17]); // 16-17 Protection status

  // // See if there are any new faults.  If so, then increment the count.
  // if (has_new_fault_occured(0))  { m_fault_count.single_cell_overvoltage_protection    += 1; }
  // if (has_new_fault_occured(1))  { m_fault_count.single_cell_undervoltage_protection   += 1; }
  // if (has_new_fault_occured(2))  { m_fault_count.whole_pack_undervoltage_protection    += 1; }
  // if (has_new_fault_occured(3))  { m_fault_count.single_cell_overvoltage_protection    += 1; }
  // if (has_new_fault_occured(4))  { m_fault_count.charging_over_temperature_protection  += 1; }
  // if (has_new_fault_occured(5))  { m_fault_count.charging_low_temperature_protection   += 1; }
  // if (has_new_fault_occured(6))  { m_fault_count.discharge_over_temperature_protection += 1; }
  // if (has_new_fault_occured(7))  { m_fault_count.discharge_low_temperature_protection  += 1; }
  // if (has_new_fault_occured(8))  { m_fault_count.charging_overcurrent_protection       += 1; }
  // if (has_new_fault_occured(9))  { m_fault_count.discharge_overcurrent_protection      += 1; }
  // if (has_new_fault_occured(10)) { m_fault_count.short_circuit_protection              += 1; }
  // if (has_new_fault_occured(11)) { m_fault_count.front_end_detection_ic_error          += 1; }
  // if (has_new_fault_occured(12)) { m_fault_count.software_lock_mos                     += 1; }

  // m_0x03_basic_info.software_version = m_rx_data[18];  // 18    Software version
  // m_0x03_basic_info.remaining_soc    = m_rx_data[19];  // 19    Remaining state of charge
  // m_0x03_basic_info.mosfet_status    = m_rx_data[20];  // 20    MOSFET status
  // m_0x03_basic_info.num_cells        = m_rx_data[21];  // 21    # of batteries in series
  // m_0x03_basic_info.num_ntcs         = m_rx_data[22];  // 22    # of NTCs

  for (uint8_t i = 0; i < 2; i++)
  {
    uint8_t ntc_index = 23 + (i * 2);
    bmsInfo.NTC[i] = ((uint16_t)(rbuffer[ntc_index] << 8) | (rbuffer[ntc_index + 1])) * 0.1; // 23-24, 25-26 NTC temperature (0.1 degrees K per LSB)
    bmsInfo.NTC[i] -= 273.15;                                                                // Convert Kelvin to Celsius
  }
}

void handle_rx_0x04()
{
  uint16_t minC = 5000, maxC = 0;
  for (uint8_t i = 0; i < 4; i++)
  {
    uint16_t cellmV = (uint16_t)(rbuffer[i * 2] << 8) | rbuffer[(i * 2) + 1];
    bmsInfo.CellVoltages[i] = cellmV * 0.001; // 0-1, 2-3, 4-5, 6-7 Cell voltages (mV per LSB)
    minC = min(minC, cellmV);
    maxC = max(maxC, cellmV);
  }
  bmsInfo.CellDiff = maxC - minC;
}

void BMSProcessFrame()
{
  // find starting byte, flush
  while (rbuffer.first() != BMS_STARTBYTE)
  {
    if (!rbuffer.available())
      return;
    rbuffer.shift();
  }

  // we might have a frame
  // sanity check, minimum size
  if (rbuffer.available() < 27)
  {
    rbuffer.shift(); // invalidate for next iteration
    return;
  }

  // parse the header
  bmsPacketHeaderStruct header;
  uint8_t *byteArray = (uint8_t *)&header;
  for (size_t i = 0; i < sizeof(bmsPacketHeaderStruct); i++)
    byteArray[i] = rbuffer.shift();

  // stop byte should be at
  if (rbuffer[header.dataLen + 1] != BMS_STOPBYTE)
  {
    log_e("Stoppbyte error");
    return;
  }

  // we have a full frame

  // uint8_t checksumLen = header.dataLen + 2; // status + data len + data
  // header 0xDD and command type are skipped
  byte calcChecksum = header.status + header.dataLen;
  for (size_t i = 0; i < header.dataLen; i++)
    calcChecksum += rbuffer[i];
  calcChecksum = ((calcChecksum ^ 0xFF) + 1) & 0xFF;
  log_d("calculated calcChecksum: %x", calcChecksum);

  uint8_t rxChecksum = rbuffer[header.dataLen + 1];
  if (calcChecksum != rxChecksum)
  {
    log_e("Packet is not valid%x, expected value: %x\n", rxChecksum, calcChecksum);
    return;
  }

  switch (header.type)
  {
  case BMS_REG_BASIC_SYSTEM_INFO:
    handle_rx_0x03();
    xTaskNotify(vTFT_Task_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits);
    break;

  case BMS_REG_CELL_VOLTAGES:
    handle_rx_0x04();
    xTaskNotify(vTFT_Task_hdl, NotificationBits::CELL_UPDATE_BIT, eSetBits);
    break;

  default:
    log_e("Unknown packet type: %x", header.type);
    break;
  }

  // remove the frame from the buffer
  while (rbuffer.shift() != BMS_STOPBYTE)
  {
    if (!rbuffer.available())
      return;
  }
  return;
}

void vBMSProcessTask(void *parameter)
{
  while (true)
  {
    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, NULL, portMAX_DELAY) == pdFALSE)
      continue;
    BMSProcessFrame();
  }
  vTaskDelete(NULL);
}
