#pragma once

/**
 * @file    FesNode.hpp
 * @brief   FES (Fire Extinguisher System) — Battery Monitor & LoRaWAN Telemetry
 *
 * Hardware:
 *   - RRC2037 Smart Battery  (SBS 1.1, I2C addr 0x0B, big-endian registers)
 *   - RAK11160 LoRaWAN module (UART AT-command interface via USART1)
 *
 * I2C registers used (from I2C-Secondary Register Definition slide):
 *   0x40  REG_BATTERY_SOC      1 byte   — state of charge (%)
 *   0x42  REG_BATTERY_TEMP     2 bytes  — temperature (unit: 0.1 K)
 *   0x4A  REG_BATTERY_STATUS   2 bytes  — status / alarm flags
 *   0x4E  REG_BATTERY_VOLTAGE  2 bytes  — voltage (mV)
 *   0x50  REG_BATTERY_CURRENT  2 bytes  — current (mA, signed)
 *
 * I2C read sequence:
 *   START → Addr+W → RegAddr → REPEATED-START → Addr+R → Data[N] → STOP
 *   All multi-byte values: Big-Endian (MSB first, byte_3 → byte_0)
 *
 * LoRa packet (15 bytes, little-endian packed struct):
 *   [0]     Module ID       uint8
 *   [1-2]   Sequence        uint16
 *   [3-4]   Voltage  mV     uint16
 *   [5-6]   Current  mA     int16   (negative = charging)
 *   [7-8]   Temp ×10 °C     int16
 *   [9]     SOC %           uint8
 *   [10]    Alarm flags     uint8
 *   [11-14] Timestamp ms    uint32
 *
 * Battery update cycle: 10 s (per specification)
 *
 * @board   STM32L072CZY6TR
 * @fw      STM32CubeIDE / HAL
 */

#include "main.h"
#include <cstdint>
#include <cstring>

// ─────────────────────────────────────────────
//  I2C — RRC Battery
// ─────────────────────────────────────────────
#define FES_I2C_HANDLE              hi2c1
#define FES_I2C_TIMEOUT_MS          50U

// SBS 1.1 mandates fixed 7-bit address 0x0B for all smart batteries
#define RRC_BATTERY_I2C_ADDR        0x0BU

// Register map (I2C-Secondary Register Definition)
#define RRC_REG_BATTERY_SOC         0x40U
#define RRC_REG_BATTERY_CAPACITY    0x41U
#define RRC_REG_BATTERY_TEMP        0x42U
#define RRC_REG_BATTERY_CYCLE_COUNT 0x44U
#define RRC_REG_BATTERY_STATUS      0x4AU
#define RRC_REG_BATTERY_SERIAL_NO   0x4CU
#define RRC_REG_BATTERY_VOLTAGE     0x4EU
#define RRC_REG_BATTERY_CURRENT     0x50U
#define RRC_REG_MANUFACTURING_DATE  0x52U
#define RRC_REG_FIRMWARE_VERSION    0x56U
#define RRC_REG_SELFCHECK_STATUS    0x5AU

// Temperature: raw is in 0.1 K units → °C = (raw × 0.1) − 273.15
// Stored as ×10 °C integer: temp_dC = raw − 2731  (avoids float)
#define RRC_TEMP_OFFSET_DK          2731    // 273.1 K × 10

// ─────────────────────────────────────────────
//  UART — RAK11160 LoRaWAN
// ─────────────────────────────────────────────
#define RAK_UART_HANDLE             huart2//huart1
#define RAK_UART_TX_TIMEOUT_MS      500U
#define RAK_UART_RX_TIMEOUT_MS      8000U   // join / confirmed TX can be slow
#define RAK_AT_BUF_SIZE             256U

// AT commands
#define RAK_CMD_TEST                "AT\r\n"
#define RAK_CMD_RESET               "ATZ\r\n"
#define RAK_CMD_SET_NWM             "AT+NWM=1\r\n"       // LoRaWAN mode
#define RAK_CMD_SET_BAND            "AT+BAND=4\r\n"      // EU868
#define RAK_CMD_SET_CLASS           "AT+CLASS=A\r\n"     // class A
#define RAK_CMD_SET_ADR             "AT+ADR=1\r\n"       // adaptive data rate on
#define RAK_CMD_JOIN                "AT+JOIN=1:0:10:8\r\n" // OTAA, 8 retries
#define RAK_CMD_JOIN_QUERY          "AT+JOIN=?\r\n"
#define RAK_CMD_SEND_PREFIX         "AT+SEND=2:"         // unconfirmed, port 2
#define RAK_RESP_OK                 "OK"
#define RAK_RESP_JOINED             "+EVT:JOINED"
#define RAK_RESP_JOIN_FAIL          "+EVT:JOIN_FAILED"
#define RAK_RESP_TX_DONE            "+EVT:SEND_CONFIRMED"

// ───────────────────────ö──────────────────────
//  Alarm thresholds (aerospace defaults)
// ─────────────────────────────────────────────
// Voltage: design max is 7200 mV (from live data: battery_design_voltage)
//#define FES_THRESH_V_LOW_MV         6000U    // < 6.0 V  → undervoltage
//#define FES_THRESH_V_HIGH_MV        8600U    // > 8.6 V  → overvoltage

//  for the mock testing only. must be deleted when the Real Battery RRC inserted ( small lithium pack, ~7.2V nominal, and 4A)
#define FES_THRESH_V_LOW_MV      20000U   // < 20 V  (mock low end)
#define FES_THRESH_V_HIGH_MV     32000U   // > 32 V  (mock high end alarm)
#define FES_THRESH_I_HIGH_MA     25000U   // > 25 A  (mock high end alarm)
// Current: charging shown as 2030 mA — alarm on heavy discharge
//#define FES_THRESH_I_HIGH_MA        4000U    // > 4.0 A discharge → overcurrent
// Temperature
#define FES_THRESH_T_HIGH_DC         850     //  85.0 °C (×10)
#define FES_THRESH_T_LOW_DC         -400     // −40.0 °C (×10)
// State of charge
#define FES_THRESH_SOC_LOW_PCT       20U     // < 20 % → low battery

// ─────────────────────────────────────────────
//  Packet definition (15 bytes)
// ─────────────────────────────────────────────
#define FES_PACKET_SIZE             15U

#pragma pack(push, 1)
struct FesPacket {
    uint8_t  moduleId;      // [0]
    uint16_t sequence;      // [1-2]
    uint16_t voltage_mV;    // [3-4]
    int16_t  current_mA;    // [5-6]  negative = charging
    int16_t  temp_dC;       // [7-8]  ×10 °C
    uint8_t  soc_pct;       // [9]
    uint8_t  alarmFlags;    // [10]
    uint32_t timestampMs;   // [11-14]
};
#pragma pack(pop)

static_assert(sizeof(FesPacket) == FES_PACKET_SIZE, "FesPacket size mismatch");

// ─────────────────────────────────────────────
//  Alarm flag bits
// ─────────────────────────────────────────────
enum FesAlarm : uint8_t {
    ALARM_NONE          = 0x00,
    ALARM_OVERVOLTAGE   = 0x01,
    ALARM_UNDERVOLTAGE  = 0x02,
    ALARM_OVERCURRENT   = 0x04,
    ALARM_OVERTEMP      = 0x08,
    ALARM_UNDERTEMP     = 0x10,
    ALARM_LOW_SOC       = 0x20,
    ALARM_SENSOR_FAULT  = 0x40,
    ALARM_LORA_FAULT    = 0x80,
};

// ─────────────────────────────────────────────
//  Return codes
// ─────────────────────────────────────────────
enum class FesStatus : uint8_t {
    OK              = 0,
    ERR_I2C         = 1,
    ERR_RRC         = 2,
    ERR_RAK_TIMEOUT = 3,
    ERR_RAK_NO_JOIN = 4,
    ERR_RAK_TX      = 5,
    ERR_TIMEOUT     = 6,
};

// ─────────────────────────────────────────────
//  Live measurement struct
// ─────────────────────────────────────────────
struct FesMeasurement {
    uint16_t voltage_mV;
    int16_t  current_mA;
    float    temperatureC;
    int16_t  temp_dC;
    uint8_t  soc_pct;
    uint16_t batteryStatus;   // raw 0x4A register for diagnostics
    uint8_t  alarmFlags;
    uint32_t timestampMs;
    bool     valid;
};

// ─────────────────────────────────────────────
//  FesNode
// ─────────────────────────────────────────────
class FesNode {
public:
    explicit FesNode(uint8_t moduleId);

    FesStatus       init();
    FesMeasurement  measure();
    FesStatus       transmit(const FesMeasurement& m);
    FesStatus       measureAndTransmit();

    void setVoltageThresholds(uint16_t lowMv, uint16_t highMv);
    void setCurrentThreshold(uint16_t highMa);
    void setTempThresholds(int16_t lowDc, int16_t highDc);
    void setSocThreshold(uint8_t lowPct);

    uint16_t    sequenceNumber() const { return _sequence; }
    uint8_t     moduleId()       const { return _moduleId; }
    bool        isJoined()       const { return _joined;   }
    static const char* statusStr(FesStatus s);

private:
    // RRC battery I2C
    FesStatus rrcInit();
    FesStatus rrcReadU8 (uint8_t reg, uint8_t&  out);
    FesStatus rrcReadU16(uint8_t reg, uint16_t& out);
    FesStatus rrcReadI16(uint8_t reg, int16_t&  out);
    FesStatus i2cSbsRead(uint8_t reg, uint8_t* buf, uint16_t len);

    // RAK11160 UART
    FesStatus rakInit();
    FesStatus rakSendCmd(const char* cmd);
    FesStatus rakWaitFor(const char* token, uint32_t timeoutMs);
    FesStatus rakSendHexPayload(const uint8_t* data, uint8_t len);
    void      rakFlushRx();

    // Alarms
    uint8_t evaluateAlarms(uint16_t mV, int16_t mA,
                           int16_t dC, uint8_t soc) const;

    // State
    uint8_t  _moduleId;
    uint16_t _sequence  = 0;
    bool     _joined    = false;

    uint16_t _thVoltLow  = FES_THRESH_V_LOW_MV;
    uint16_t _thVoltHigh = FES_THRESH_V_HIGH_MV;
    uint16_t _thCurrHigh = FES_THRESH_I_HIGH_MA;
    int16_t  _thTempLow  = FES_THRESH_T_LOW_DC;
    int16_t  _thTempHigh = FES_THRESH_T_HIGH_DC;
    uint8_t  _thSocLow   = FES_THRESH_SOC_LOW_PCT;

    char _rakRxBuf[RAK_AT_BUF_SIZE];
};
