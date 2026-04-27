#pragma once

/**
 * @file    fes_led_test.hpp
 * @brief   FES breadboard test harness — STM32L072CZY6TR
 *
 * ── Available hardware ────────────────────────────────────────────────────
 *   STM32L072CZY6TR on breadboard
 *   1× Green LED  +  1 kΩ resistor  →  PA8
 *   1× Red   LED  +  1 kΩ resistor  →  PA9
 *   Jumper wires, USB-UART dongle on PA2 (USART2 TX) for debug
 *
 * ── Breadboard wiring ────────────────────────────────────────────────────
 *
 *   PA8 ──[1kΩ]──[GREEN LED +]──[GREEN LED −]── GND
 *   PA9 ──[1kΩ]──[RED   LED +]──[RED   LED −]── GND
 *   PA2 ──────────────────────────────────────── USB-UART RX (optional)
 *
 *   LED orientation: longer leg (anode) faces the MCU / resistor side.
 *
 * ── CubeMX additions for this test ───────────────────────────────────────
 *   PA8 → GPIO_Output, label LED_GREEN, initial level Low
 *   PA9 → GPIO_Output, label LED_RED,   initial level Low
 *   (USART2 already at 115200 for debug)
 *
 * ── Mock mode (no sensors needed) ────────────────────────────────────────
 *   Project → Properties → C/C++ Build → Settings →
 *   MCU GCC Compiler → Preprocessor → add symbol:  FES_MOCK_SENSORS
 *
 * ── LED behaviour ─────────────────────────────────────────────────────────
 *   Event                         Green          Red
 *   ──────────────────────────    ─────────────  ──────────────────
 *   Power-on self-test            Flash once     Flash once
 *   Sensor read OK, no alarms     Solid 1 s      Off
 *   Alarm threshold exceeded      Off            Solid 1 s
 *   LoRa TX success               3× blink       Off
 *   I2C / sensor fault            Off            5× fast blink
 */

#pragma once

#define LED_GREEN_PORT      GPIOA
#define LED_GREEN_PIN       GPIO_PIN_8

#define LED_RED_PORT        GPIOA
#define LED_RED_PIN         GPIO_PIN_9
//using the available led on the MCU board
/*
#define LED_GREEN_PORT      GPIOA
#define LED_GREENA5_PIN       GPIO_PIN_5

#define LED_RED_PORT        GPIOA
#define LED_GREENA5_PIN        GPIO_PIN_5
*/
#define LED_BLINK_SHORT_MS  100U
#define LED_BLINK_LONG_MS   500U

#ifdef __cplusplus
extern "C" {
#endif

void fes_test_init(void);
void fes_test_run(void);

#ifdef __cplusplus
}
#endif



