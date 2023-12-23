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

CircularBuffer<uint8_t, 120> rbuffer;
TaskHandle_t vBMSProcess_Task_hdl;
 TickType_t xBmsRequestLastCall;

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

  // Serial.print("intoCircularBuffer: ");
  // for (size_t i = 0; i < length; i++)
  //   Serial.printf("%02X ", pData[i]);
  // Serial.println();

  if (pData[length - 1] == BMS_STOPBYTE)
  {
    // we have a frame
    log_i("buffer usage: %d", rbuffer.size());
    xTaskNotify(vBMSProcess_Task_hdl, 0, eNoAction);
  }
}

void handle_rx_0x03()
{

  bmsInfo.Voltage = ((uint16_t)rbuffer[0] << 8 | rbuffer[1]) * 0.01;         // 0-1   Total voltage
  bmsInfo.Current = ((int16_t)rbuffer[2] << 8 | (int16_t)rbuffer[3]) * 0.01; // 2-3   Current
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
  bmsInfo.stateFET = rbuffer[20];
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
      xBmsRequestLastCall= xTaskGetTickCount ();
    break;

  case BMS_REG_CELL_VOLTAGES:
    handle_rx_0x04();
    xTaskNotify(vTFT_Task_hdl, NotificationBits::CELL_UPDATE_BIT, eSetBits);
    break;

  case BMS_REG_CTL_MOSFET:
    log_d("MOSFET status: %02X", rbuffer[1]);
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

    // next poll, grace period
    vTaskDelay(pdMS_TO_TICKS(BMS_UPDATE_DELAY));
    vTaskDelayUntil(&bmsUpdateCouter, pdMS_TO_TICKS(BMS_UPDATE_DELAY));
    xTaskNotify(vBMS_Polling_hdl, NotificationBits::BMS_UPDATE_BIT, eSetBits); // next poll
  }
  vTaskDelete(NULL);
}

uint16_t calcChecksum(std::vector<uint8_t> *data)
{
  uint16_t checksum = 0x00;
  for (size_t i = 2; i < data->size() - 3; i++)
    checksum -= data->at(i);

  return checksum;
}

void addChecksum(std::vector<uint8_t> *data)
{
  uint16_t checksum = calcChecksum(data);

  data->at(data->size() - 1 - 2) = checksum >> 8;
  data->at(data->size() - 1 - 1) = checksum >> 0;
}

// header status command length data calcChecksum footer
//   DD     A5      03     00    FF     FD      77
// uint8_t basicRequest[] = {0xdd, 0xa5, 0x3, 0x0, 0xff, 0xfd, 0x77};
// uint8_t cellInfoRequest[] = {0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77};

std::vector<uint8_t> basicRequest()
{
  std::vector<uint8_t> r = {BMS_STARTBYTE, BMS_READ, BMS_REG_BASIC_SYSTEM_INFO, 0, 0x0, 0x0, BMS_STOPBYTE};
  addChecksum(&r);
  log_d("%02X %02X %02X %02X %02X %02X %02X",
        r[0], r[1], r[2], r[3], r[4], r[5], r[6]);
  return r;
}

std::vector<uint8_t> cellInfoRequest()
{
  std::vector<uint8_t> r = {BMS_STARTBYTE, BMS_READ, BMS_REG_CELL_VOLTAGES, 0, 0x0, 0x0, BMS_STOPBYTE};
  addChecksum(&r);
  log_d("%02X %02X %02X %02X %02X %02X %02X",
        r[0], r[1], r[2], r[3], r[4], r[5], r[6]);
  return r;
}

// DD 5A E1 02 00 02 FF 1B 77, it means software shutdown discharging MOS.
std::vector<uint8_t> mosfetChargeCtrlRequest(bool charge)
{
  std::vector<uint8_t> r = {BMS_STARTBYTE, BMS_WRITE, BMS_REG_CTL_MOSFET, 2, 0x0, 0x0, 0x0, 0x0, BMS_STOPBYTE};
  uint8_t state = 0;

  if (charge)
    state = 0b00;
  else
    state = 0b01;
  r[5] = state;
  addChecksum(&r);
  log_d("%02X %02X %02X %02X %02X %02X %02X %02X %02X",
        r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]);
  return r;
}
