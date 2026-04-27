#pragma once

/**
 * @file    FesNode_Mock.hpp
 * @brief   Software mock for FES sensor testing — no RRC battery needed for instance
 *
 * HOW TO ENABLE
 * ─────────────
 * In STM32CubeIDE:
 *   Right-click project → Properties →
 *   C/C++ Build → Settings → MCU GCC Compiler → Preprocessor →
 *   Defined symbols → click [+] → type:  FES_MOCK_SENSORS
 *
 * Then rebuild.
 * Note: Remove the symbol when real hardware is connected.
 *
 *
 *
 * WHAT IT GENERATES  (based on HAL_GetTick() elapsed time)
 * ──────────────────────────────────────────────────────────
 *   Voltage:     20–35 V  sine wave, 60 s full cycle
 *                → crosses 30 V threshold → ALARM_OVERVOLTAGE fires
 *
 *   Current:     0→30 A   sawtooth ramp, 40 s full cycle
 *                → crosses 25 A threshold → ALARM_OVERCURRENT fires
 *
 *   Temperature: 20–95 °C sine wave, 80 s full cycle
 *                → crosses 85 °C threshold → ALARM_OVERTEMP fires
 *
 * All five alarm conditions (including LOW_SOC) trigger automatically
 * within ~80 seconds so every LED pattern can be observed.
 *
 * HOW TO WIRE INTO FesNode.cpp?
 * ────────────────────────────
 * In FesNode.cpp, wrap the real sensor method bodies with
 * #ifdef / #else / #endif as shown below.
 * The mock returns FesStatus::OK and fills in fake values.
 *
 * Example — rrcReadU16() for voltage:
 *
 *   FesStatus FesNode::rrcReadU16(uint8_t reg, uint16_t& out) {
 *   #ifdef FES_MOCK_SENSORS
 *       switch (reg) {
 *           case RRC_REG_BATTERY_VOLTAGE:
 *               out = static_cast<uint16_t>(FesMock::voltage_mV());
 *               return FesStatus::OK;
 *           case RRC_REG_BATTERY_TEMP:
 *               // convert °C back to raw 0.1 K units for the mock
 *               out = static_cast<uint16_t>(
 *                     (FesMock::temperature_C() * 10.0f) + 2731.0f);
 *               return FesStatus::OK;
 *           case RRC_REG_BATTERY_STATUS:
 *               out = 0x0000;
 *               return FesStatus::OK;
 *           default:
 *               out = 0;
 *               return FesStatus::OK;
 *       }
 *   #else
 *       // ... real I2C implementation ...
 *   #endif
 *   }
 *
 *   FesStatus FesNode::rrcReadI16(uint8_t reg, int16_t& out) {
 *   #ifdef FES_MOCK_SENSORS
 *       if (reg == RRC_REG_BATTERY_CURRENT) {
 *           out = static_cast<int16_t>(FesMock::current_mA());
 *           return FesStatus::OK;
 *       }
 *       out = 0;
 *       return FesStatus::OK;
 *   #else
 *       // ... real I2C implementation ...
 *   #endif
 *   }
 *
 *   FesStatus FesNode::rrcReadU8(uint8_t reg, uint8_t& out) {
 *   #ifdef FES_MOCK_SENSORS
 *       if (reg == RRC_REG_BATTERY_SOC) {
 *           out = FesMock::soc_pct();
 *           return FesStatus::OK;
 *       }
 *       out = 0;
 *       return FesStatus::OK;
 *   #else
 *       // ... real I2C implementation ...
 *   #endif
 *   }
 *
 *   FesStatus FesNode::rrcInit() {
 *   #ifdef FES_MOCK_SENSORS
 *       return FesStatus::OK;   // no I2C needed
 *   #else
 *       // ... real implementation ...
 *   #endif
 *   }
 */

#ifdef FES_MOCK_SENSORS

#include "main.h"   // HAL_GetTick()
#include <cmath>

namespace FesMock {

    /**
     * Voltage: 20000–35000 mV sine wave, 60 s period.
     * Crosses the 30 V alarm threshold twice per cycle.
     *  range is 20000–35000 mV (sine wave centred at 27500, amplitude 7500).
     */
    inline float voltage_mV()
    {
        const float t = static_cast<float>(HAL_GetTick()) / 1000.0f;
        return 27500.0f + 7500.0f * sinf(t * 2.0f * 3.14159f / 60.0f);
    }

    /**
     * Current: 0–30000 mA sawtooth ramp, 40 s period.
     * Crosses the 25 A alarm threshold once per cycle.
     * Positive = discharging, negative would be charging.
     */
    inline float current_mA()
    {
        const float t     = static_cast<float>(HAL_GetTick()) / 1000.0f;
        const float phase = fmodf(t, 40.0f) / 40.0f;   // 0.0 → 1.0
        return 30000.0f * phase;
    }
    /**
     * Temperature: 20–95 °C sine wave, 80 s period.
     * Crosses the 85 °C alarm threshold twice per cycle.
     */
    inline float temperature_C()
    {
        const float t = static_cast<float>(HAL_GetTick()) / 1000.0f;
        return 57.5f + 37.5f * sinf(t * 2.0f * 3.14159f / 80.0f);
    }

    /**
     * State of charge: 100 → 0 % over 120 s, then resets.
     * Crosses the 20 % LOW_SOC threshold near end of cycle.
     */
    inline uint8_t soc_pct()
    {
        const float t     = static_cast<float>(HAL_GetTick()) / 1000.0f;
        const float phase = fmodf(t, 120.0f) / 120.0f;  // 0.0 → 1.0
        const int   soc   = 100 - static_cast<int>(phase * 100.0f);
        return static_cast<uint8_t>(soc < 0 ? 0 : soc);
    }

}  // namespace FesMock

#endif  // FES_MOCK_SENSOR
