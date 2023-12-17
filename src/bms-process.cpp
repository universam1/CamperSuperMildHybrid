#include "smarthybrid.h"

typedef struct
{
    byte start;
    byte type;
    byte status;
    byte dataLen;
} bmsPacketHeaderStruct;

typedef struct
{
    uint16_t Volts; // unit 1mV
    int32_t Amps;   // unit 1mA
    int32_t Watts;  // unit 1W
    uint16_t CapacityRemainAh;
    uint8_t CapacityRemainPercent; // unit 1%
    uint32_t CapacityRemainWh;     // unit Wh
    uint16_t Temp1;                // unit 0.1C
    uint16_t Temp2;                // unit 0.1C
    uint16_t BalanceCodeLow;
    uint16_t BalanceCodeHigh;
    uint8_t MosfetStatus;

} packBasicInfoStruct;

typedef struct
{
    uint8_t NumOfCells;
    uint16_t CellVolt[15]; // cell 1 has index 0 :-/
    uint16_t CellMax;
    uint16_t CellMin;
    uint16_t CellDiff; // difference between highest and lowest
    uint16_t CellAvg;
    uint16_t CellMedian;
    uint32_t CellColor[15];
    uint32_t CellColorDisbalance[15]; // green cell == median, red/violet cell => median + c_cellMaxDisbalance
} packCellInfoStruct;

struct packEepromStruct
{
    uint16_t POVP;
    uint16_t PUVP;
    uint16_t COVP;
    uint16_t CUVP;
    uint16_t POVPRelease;
    uint16_t PUVPRelease;
    uint16_t COVPRelease;
    uint16_t CUVPRelease;
    uint16_t CHGOC;
    uint16_t DSGOC;
};

#define STRINGBUFFERSIZE 300
char stringBuffer[STRINGBUFFERSIZE];

const int32_t c_cellNominalVoltage = 3700; // mV

const uint16_t c_cellAbsMin = 3000;
const uint16_t c_cellAbsMax = 4200;

const int32_t c_packMaxWatt = 1250;

const uint16_t c_cellMaxDisbalance = 1500; // 200; // cell different by this value from cell median is getting violet (worst) color
// const byte cBasicInfo3 = 3; //type of packet 3= basic info
// const byte cCellInfo4 = 4;  //type of packet 4= individual cell info

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

bool isPacketValid(byte *packet) // check if packet is valid
{
    if (packet == nullptr)
    {
        return false;
    }

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    int checksumLen = pHeader->dataLen + 2; // status + data len + data

    if (pHeader->start != 0xDD)
    {
        return false;
    }

    int offset = 2; // header 0xDD and command type are skipped

    byte calcChecksum = 0;
    for (int i = 0; i < checksumLen; i++)
    {
        calcChecksum += packet[offset + i];
    }

    // printf("calcChecksum: %x\n", calcChecksum);

    calcChecksum = ((calcChecksum ^ 0xFF) + 1) & 0xFF;
    // printf("calcChecksum v2: %x\n", calcChecksum);

    byte rxChecksum = packet[offset + checksumLen + 1];

    if (calcChecksum == rxChecksum)
    {
        // printf("Packet is valid\n");
        return true;
    }
    else
    {
        // printf("Packet is not valid\n");
        // printf("Expected value: %x\n", rxChecksum);
        return false;
    }
}

bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen)
{
    // Expected data len
    if (dataLen != 0x1B)
    {
        return false;
    }

    output->Volts = ((uint32_t)two_ints_into16(data[0], data[1])) * 10; // Resolution 10 mV -> convert to milivolts   eg 4895 > 48950mV
    output->Amps = ((int32_t)two_ints_into16(data[2], data[3])) * 10;   // Resolution 10 mA -> convert to miliamps

    output->Watts = output->Volts * output->Amps / 1000000; // W

    output->CapacityRemainAh = ((uint16_t)two_ints_into16(data[4], data[5])) * 10;
    output->CapacityRemainPercent = ((uint8_t)data[19]);

    output->CapacityRemainWh = (output->CapacityRemainAh * c_cellNominalVoltage) / 1000000 * packCellInfo.NumOfCells;

    output->Temp1 = (((uint16_t)two_ints_into16(data[23], data[24])) - 2731);
    output->Temp2 = (((uint16_t)two_ints_into16(data[25], data[26])) - 2731);
    output->BalanceCodeLow = (two_ints_into16(data[12], data[13]));
    output->BalanceCodeHigh = (two_ints_into16(data[14], data[15]));
    output->MosfetStatus = ((byte)data[20]);

    return true;
};

bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen)
{

    uint16_t _cellSum;
    uint16_t _cellMin = 5000;
    uint16_t _cellMax = 0;
    uint16_t _cellAvg;
    uint16_t _cellDiff;

    output->NumOfCells = dataLen / 2; // Data length * 2 is number of cells !!!!!!

    // go trough individual cells
    for (byte i = 0; i < dataLen / 2; i++)
    {
        output->CellVolt[i] = ((uint16_t)two_ints_into16(data[i * 2], data[i * 2 + 1])); // Resolution 1 mV
        _cellSum += output->CellVolt[i];
        if (output->CellVolt[i] > _cellMax)
        {
            _cellMax = output->CellVolt[i];
        }
        if (output->CellVolt[i] < _cellMin)
        {
            _cellMin = output->CellVolt[i];
        }
    }
    output->CellMin = _cellMin;
    output->CellMax = _cellMax;
    output->CellDiff = _cellMax - _cellMin; // Resolution 10 mV -> convert to volts
    output->CellAvg = _cellSum / output->NumOfCells;

    return true;
};

bool bmsProcessPacket(byte *packet)
{
    bool isValid = isPacketValid(packet);

    if (isValid != true)
    {
        Serial.println("Invalid packer received");
        return false;
    }

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    byte *data = packet + sizeof(bmsPacketHeaderStruct); // TODO Fix this ugly hack
    unsigned int dataLen = pHeader->dataLen;

    bool result = false;

    // |Decision based on pac ket type (info3 or info4)
    switch (pHeader->type)
    {
    case cBasicInfo3:
    {
        // Process basic info
        result = processBasicInfo(&packBasicInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }

    case cCellInfo4:
    {
        result = processCellInfo(&packCellInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }

    default:
        result = false;
        Serial.printf("Unsupported packet type detected. Type: %d", pHeader->type);
    }

    return result;
}

#include <CircularBuffer.h>

CircularBuffer<uint8_t, 100> rbuffer; // uses 538 bytes

// into circular buffer
void toCircularBuffer(uint8_t *pData, size_t length)
{
    for (size_t i = 0; i < length; i++)
        rbuffer.push(pData[i]);
    if (pData[length - 1] == BMS_STOPBYTE)
        // we have a frame
        xTaskNotify(vBMSProcess_Task_hdl, NotificationBits::BMS_FRAME_RX_BIT, eSetBits);
}

void BMSProcessTask()
{
    uint32_t ulNotifiedValue;

    while (true)
    {
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &ulNotifiedValue, pdMS_TO_TICKS(2000));
        BMSProcessPacket();
    }
}

void BMSProcessPacket()
{
    // find starting byte, flush
    while (rbuffer.first() != BMS_STARTBYTE)
    {
        if (!rbuffer.available())
            return;
        rbuffer.shift();
    }

    // we might have a frame
    // sanity check, minium size
    if (rbuffer.available() < sizeof(packCellInfoStruct))
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
    uint8_t stopByteIdx =
        sizeof(bmsPacketHeaderStruct) +
        header.dataLen +
        sizeof(byte);
    if (rbuffer[stopByteIdx] != BMS_STOPBYTE)
    {
        log_e("Stoppbyte error");
        return;
    }

    // we have a full frame

    // uint8_t checksumLen = header.dataLen + 2; // status + data len + data
    const byte offset = 2; // header 0xDD and command type are skipped
    byte calcChecksum;
    for (size_t i = offset; i < (header.dataLen + 2); i++)
        calcChecksum += rbuffer[i];
    calcChecksum = ((calcChecksum ^ 0xFF) + 1) & 0xFF;
    log_d("calculated calcChecksum: %x", calcChecksum);

    uint8_t rxChecksum = rbuffer[offset + header.dataLen + 2 + 1];
    if (calcChecksum != rxChecksum)
    {
        log_e("Packet is not valid%x, expected value: %x\n", rxChecksum, calcChecksum);
        return;
    }

    switch (header.type)
    {
    case BMS_REG_BASIC_SYSTEM_INFO:
       __htons
        break;
    
    default:
        break;
    }
}

bool bleCollectPacket(char *data, uint32_t dataSize) // reconstruct packet from BLE incomming data, called by notifyCallback function
{
    static uint8_t packetstate = 0; // 0 - empty, 1 - first half of packet received, 2- second half of packet received
    static uint8_t packetbuff[40] = {0x0};
    static uint32_t previousDataSize = 0;
    bool retVal = false;
    // hexDump(data,dataSize);

    if (data[0] == 0xdd && packetstate == 0) // probably got 1st half of packet
    {
        packetstate = 1;
        previousDataSize = dataSize;
        for (uint8_t i = 0; i < dataSize; i++)
        {
            packetbuff[i] = data[i];
        }
        retVal = false;
    }

    if (data[dataSize - 1] == 0x77 && packetstate == 1) // probably got 2nd half of the packet
    {
        packetstate = 2;
        for (uint8_t i = 0; i < dataSize; i++)
        {
            packetbuff[i + previousDataSize] = data[i];
        }
        retVal = false;
    }

    if (packetstate == 2) // got full packet
    {
        uint8_t packet[dataSize + previousDataSize];
        memcpy(packet, packetbuff, dataSize + previousDataSize);

        bmsProcessPacket(packet); // pass pointer to retrieved packet to processing function
        packetstate = 0;
        retVal = true;
    }
    return retVal;
}
void bmsGetInfo3()
{
    // header status command length data calcChecksum footer
    //   DD     A5      03     00    FF     FD      77
    uint8_t data[7] = {0xdd, 0xa5, 0x3, 0x0, 0xff, 0xfd, 0x77};
    // bmsSerial.write(data, 7);
    sendCommand(data, sizeof(data));
    // Serial.println("Request info3 sent");
}

void bmsGetInfo4()
{
    //  DD  A5 04 00  FF  FC  77
    uint8_t data[7] = {0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77};
    // bmsSerial.write(data, 7);
    sendCommand(data, sizeof(data));
    // Serial.println("Request info4 sent");
}

void printBasicInfo() // debug all data to uart
{
    Serial.printf("Total voltage: %f\n", (float)packBasicInfo.Volts / 1000);
    Serial.printf("Amps: %f\n", (float)packBasicInfo.Amps / 1000);
    Serial.printf("CapacityRemainAh: %f\n", (float)packBasicInfo.CapacityRemainAh / 1000);
    Serial.printf("CapacityRemainPercent: %d\n", packBasicInfo.CapacityRemainPercent);
    Serial.printf("Temp1: %f\n", (float)packBasicInfo.Temp1 / 10);
    Serial.printf("Temp2: %f\n", (float)packBasicInfo.Temp2 / 10);
    Serial.printf("Balance Code Low: 0x%x\n", packBasicInfo.BalanceCodeLow);
    Serial.printf("Balance Code High: 0x%x\n", packBasicInfo.BalanceCodeHigh);
    Serial.printf("Mosfet Status: 0x%x\n", packBasicInfo.MosfetStatus);
}

void printCellInfo() // debug all data to uart
{
    Serial.printf("Number of cells: %u\n", packCellInfo.NumOfCells);
    for (byte i = 1; i <= packCellInfo.NumOfCells; i++)
    {
        Serial.printf("Cell no. %u", i);
        Serial.printf("   %f\n", (float)packCellInfo.CellVolt[i - 1] / 1000);
    }
    Serial.printf("Max cell volt: %f\n", (float)packCellInfo.CellMax / 1000);
    Serial.printf("Min cell volt: %f\n", (float)packCellInfo.CellMin / 1000);
    Serial.printf("Difference cell volt: %f\n", (float)packCellInfo.CellDiff / 1000);
    Serial.printf("Average cell volt: %f\n", (float)packCellInfo.CellAvg / 1000);
    Serial.printf("Median cell volt: %f\n", (float)packCellInfo.CellMedian / 1000);
    Serial.println();
}

void hexDump(const char *data, uint32_t dataSize) // debug function
{
    Serial.println("HEX data:");

    for (int i = 0; i < dataSize; i++)
    {
        Serial.printf("0x%x, ", data[i]);
    }
    Serial.println("");
}

int16_t two_ints_into16(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
    int16_t result = (highbyte);
    result <<= 8;                // Left shift 8 bits,
    result = (result | lowbyte); // OR operation, merge the two
    return result;
}

void constructBigString() // debug all data to uart
{
    stringBuffer[0] = '\0'; // clear old data
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Total voltage: %f\n", (float)packBasicInfo.Volts / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Amps: %f\n", (float)packBasicInfo.Amps / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "CapacityRemainAh: %f\n", (float)packBasicInfo.CapacityRemainAh / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "CapacityRemainPercent: %d\n", packBasicInfo.CapacityRemainPercent);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Temp1: %f\n", (float)packBasicInfo.Temp1 / 10);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Temp2: %f\n", (float)packBasicInfo.Temp2 / 10);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Balance Code Low: 0x%x\n", packBasicInfo.BalanceCodeLow);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Balance Code High: 0x%x\n", packBasicInfo.BalanceCodeHigh);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Mosfet Status: 0x%x\n", packBasicInfo.MosfetStatus);

    snprintf(stringBuffer, STRINGBUFFERSIZE, "Number of cells: %u\n", packCellInfo.NumOfCells);
    for (byte i = 1; i <= packCellInfo.NumOfCells; i++)
    {
        snprintf(stringBuffer, STRINGBUFFERSIZE, "Cell no. %u", i);
        snprintf(stringBuffer, STRINGBUFFERSIZE, "   %f\n", (float)packCellInfo.CellVolt[i - 1] / 1000);
    }
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Max cell volt: %f\n", (float)packCellInfo.CellMax / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Min cell volt: %f\n", (float)packCellInfo.CellMin / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Difference cell volt: %f\n", (float)packCellInfo.CellDiff / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Average cell volt: %f\n", (float)packCellInfo.CellAvg / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "Median cell volt: %f\n", (float)packCellInfo.CellMedian / 1000);
    snprintf(stringBuffer, STRINGBUFFERSIZE, "\n");
}