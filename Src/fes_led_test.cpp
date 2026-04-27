/**
 * @file    fes_led_test.cpp
 * @brief   FES breadboard LED test — STM32L072CZY6TR
 *
 * Usage in main.cpp:
 *   extern "C" void fes_test_init(void);
 *   extern "C" void fes_test_run(void);
 *
 *   // after all MX_*_Init():
 *   fes_test_init();
 *   while (1) { fes_test_run(); }
 */

#include "FesNode.hpp"
#include "fes_led_test.hpp"
#ifdef FES_MOCK_SENSORS
#include "FesNode_Mock.hpp"
#endif
#include <cstdio>
#include <cstring>

extern UART_HandleTypeDef huart2;   // PA2 debug UART

static constexpr uint32_t CYCLE_MS = 10000U;   // 10 s — matches battery spec

static FesNode fesNode(0x01);

// ── LED helpers ───────────────────────────────────────────────────────────

static inline void ledGreen(bool on)
{
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void ledRed(bool on)
{
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void blinkGreen(uint8_t n, uint32_t halfMs = LED_BLINK_LONG_MS)
{
    for (uint8_t i = 0; i < n; i++) {
        ledGreen(true);  HAL_Delay(halfMs);
        ledGreen(false); HAL_Delay(halfMs);
    }
}

static void blinkRedFast(uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) {
        ledRed(true);  HAL_Delay(LED_BLINK_SHORT_MS);
        ledRed(false); HAL_Delay(LED_BLINK_SHORT_MS);
    }
}

// ── UART debug ────────────────────────────────────────────────────────────

static void dbg(const char* msg)
{
    HAL_UART_Transmit(&huart2,
                      reinterpret_cast<const uint8_t*>(msg),
                      static_cast<uint16_t>(strlen(msg)), 200U);
}

// ─────────────────────────────────────────────────────────────────────────────
//  fes_test_init()
// ─────────────────────────────────────────────────────────────────────────────
extern "C" {
void fes_test_init(void)
{
    ledGreen(false); ledRed(false);

    dbg("\r\n=== FES LED TEST — STM32L072CZY6TR ===\r\n");
#ifdef FES_MOCK_SENSORS
    dbg("[MODE] MOCK — no real I2C hardware needed\r\n");
#else
    dbg("[MODE] REAL — RRC Battery @ I2C 0x0B\r\n");
#endif

    // Power-on self-test: both LEDs flash once
    ledGreen(true); ledRed(true);
    HAL_Delay(300);
    ledGreen(false); ledRed(false);
    HAL_Delay(200);

    const FesStatus st = fesNode.init();

    char buf[80];
    snprintf(buf, sizeof(buf), "[INIT] %s\r\n", FesNode::statusStr(st));
    dbg(buf);

    if (st != FesStatus::OK) {
        dbg("[HALT] Init failed — red fast blink loop\r\n");
        while (1) { blinkRedFast(10); HAL_Delay(500); }
    }

    dbg("[OK]   Sampling every 10 s\r\n\r\n");
}
}
// ─────────────────────────────────────────────────────────────────────────────
//  fes_test_run()  — call from while(1)
// ─────────────────────────────────────────────────────────────────────────────
extern "C"
{
 void fes_test_run(void)
{
    static uint32_t lastMs = 0;
    char buf[200];

    if ((HAL_GetTick() - lastMs) < CYCLE_MS) return;
    lastMs = HAL_GetTick();

    ledGreen(false); ledRed(false);

    // ── Measure ──────────────────────────────────────────────────────────
    const FesMeasurement m = fesNode.measure();

    if (!m.valid) {
        dbg("[ERR]  Sensor read FAILED\r\n");
        blinkRedFast(5);
        return;
    }

    snprintf(buf, sizeof(buf),
             "[DATA] seq=%-4u  V=%5u mV  I=%+6d mA  "
             "T=%+5.1f°C  SOC=%3u%%  alarms=0x%02X\r\n",
             fesNode.sequenceNumber(),
             m.voltage_mV, m.current_mA, m.temperatureC,
             m.soc_pct, m.alarmFlags);
    dbg(buf);

    // ── Alarm names ───────────────────────────────────────────────────────
    if (m.alarmFlags != ALARM_NONE) {
        dbg("[ALRM]");
        if (m.alarmFlags & ALARM_OVERVOLTAGE)  dbg(" OVERVOLTAGE");
        if (m.alarmFlags & ALARM_UNDERVOLTAGE) dbg(" UNDERVOLTAGE");
        if (m.alarmFlags & ALARM_OVERCURRENT)  dbg(" OVERCURRENT");
        if (m.alarmFlags & ALARM_OVERTEMP)     dbg(" OVERTEMP");
        if (m.alarmFlags & ALARM_UNDERTEMP)    dbg(" UNDERTEMP");
        if (m.alarmFlags & ALARM_LOW_SOC)      dbg(" LOW_SOC");
        dbg("\r\n");
    }

    // ── LED status ────────────────────────────────────────────────────────
    if (m.alarmFlags != ALARM_NONE) {
        ledRed(true);
        dbg("[LED]  RED — alarm active\r\n");
    } else {
        ledGreen(true);
        dbg("[LED]  GREEN — all OK\r\n");
    }
    HAL_Delay(1000);
    ledGreen(false); ledRed(false);

    // ── Transmit ─────────────────────────────────────────────────────────
    dbg("[LORA] Sending...\r\n");
    const FesStatus txSt = fesNode.transmit(m);
    snprintf(buf, sizeof(buf), "[LORA] %s\r\n", FesNode::statusStr(txSt));
    dbg(buf);

    if (txSt == FesStatus::OK) {
        blinkGreen(3);
        dbg("[LED]  GREEN 3× — TX OK\r\n");
    } else {
        ledRed(true); HAL_Delay(500); ledRed(false);
        dbg("[LED]  RED blink — TX failed\r\n");
    }

    dbg("\r\n");
}
}


