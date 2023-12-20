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
  {
    // we have a frame
    log_i("buffer usage: %d", rbuffer.size());
    xTaskNotify(vBMSProcess_Task_hdl, 0, eNoAction);
  }
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
  //   bmsInfo.mosfet_status    = rbuffer[20];  // 20    MOSFET status
  // m_0x03_basic_info.num_cells        = m_rx_data[21];  // 21    # of batteries in series
  // m_0x03_basic_info.num_ntcs         = m_rx_data[22];  // 22    # of NTCs
  bmsInfo.dischargeFET = (rbuffer[20] >> 1) & 1;
  bmsInfo.chargeFET = rbuffer[20] & 1;

  for (uint8_t i = 0; i < 2; i++)
  {
    uint8_t ntc_index = 23 + (i * 2);
    bmsInfo.NTC[i] = ((uint16_t)(rbuffer[ntc_index] << 8) | (rbuffer[ntc_index + 1])) * 0.1; // 23-24, 25-26 NTC temperature (0.1 degrees K per LSB)
    bmsInfo.NTC[i] -= 273.15;                                                                // Convert Kelvin to Celsius
  }
  log_i("BMS Info: %.2fV %.2fA %.2fW %.1fC %.1fC C:%d D:%d",
        bmsInfo.Voltage, bmsInfo.Current, bmsInfo.Power, bmsInfo.NTC[0], bmsInfo.NTC[1], bmsInfo.chargeFET, bmsInfo.dischargeFET);
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
  log_i("Cell voltages: %.3fV %.3fV %.3fV %.3fV diff: %dmV",
        bmsInfo.CellVoltages[0], bmsInfo.CellVoltages[1], bmsInfo.CellVoltages[2], bmsInfo.CellVoltages[3], bmsInfo.CellDiff);
}
// uint16_t calcChecksum(uint8_t *data, uint8_t len)
// {
//   uint16_t checksum = 0x00;
//   for (size_t i = 0; i < len; i++)
//     checksum = checksum - data[i];

//   return checksum;
// }
// void addChecksum(uint8_t *data, uint8_t len)
// {
//   uint16_t checksum = calcChecksum(data, len);

//   data[len - 1 - 2] = (uint8_t)((checksum >> 8) & 0xFF);
//   data[len - 1 - 1] = (uint8_t)(checksum & 0xFF);
// }
void BMSProcessFrame()
{
  // find starting byte, flush
  while (rbuffer.first() != BMS_STARTBYTE)
  {

    if (rbuffer.isEmpty())
    {
      log_d("buffer empty");
      return;
    }
    log_d("flushing %02X", rbuffer.first());
    rbuffer.shift();
  }

  // we might have a frame
  // sanity check, minimum size
  if (rbuffer.size() < 27)
  {
    log_d("buffer too small");
    rbuffer.shift(); // invalidate for next iteration
    return;
  }

  // parse the header
  bmsPacketHeaderStruct header;
  uint8_t *byteArray = (uint8_t *)&header;
  for (size_t i = 0; i < sizeof(bmsPacketHeaderStruct); i++)
    byteArray[i] = rbuffer.shift();

  // stop byte should be at
  if (rbuffer[header.dataLen + 2] != BMS_STOPBYTE)
  {
    log_d("header: %02X %02X %02X %02X", header.start, header.type, header.status, header.dataLen);
    log_d("size: %d", rbuffer.size());
    for (size_t i = 0; i < rbuffer.size(); i++)
      Serial.printf("%02X ", rbuffer[i]);
    Serial.println();
    log_e("Stoppbyte error %02X", rbuffer[header.dataLen + 1]);
    return;
  }

  // we have a full frame

  // uint8_t checksumLen = header.dataLen + 2; // status + data len + data
  // header 0xDD and command type are skipped
  uint16_t computed_crc = 0x00;
  computed_crc -= header.status;
  computed_crc -= header.dataLen;
  for (size_t i = 0; i < header.dataLen; i++)
    computed_crc -= rbuffer[i];
  log_d("calculated calcChecksum: %02X", computed_crc);

  uint16_t remote_crc = uint16_t(rbuffer[header.dataLen]) << 8 | (uint16_t(rbuffer[header.dataLen + 1]) << 0);
  if (computed_crc != remote_crc)
  {
    log_e("Packet is not valid %02X, expected value: %02X", remote_crc, computed_crc);
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
    log_e("Unknown packet type: %02X", header.type);
    break;
  }

  // remove the frame from the buffer
  while (rbuffer.shift() != BMS_STOPBYTE && !rbuffer.isEmpty())
  {
  }

  return;
}

void vBMSProcessTask(void *parameter)
{
  while (true)
  {
    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, NULL, portMAX_DELAY) == pdFALSE)
      continue;
    log_d("start processing frame");
    BMSProcessFrame();
  }
  vTaskDelete(NULL);
}
