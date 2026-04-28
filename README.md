# FES Battery Monitor — STM32L072 Mock Test Harness

A firmware prototype for an **aerospace Fire Extinguisher System (FES)** battery monitoring node, developed and validated on the **STM32L072CZY6TR Discovery board** (MB1296D). This repository contains the full mock sensor test harness — no real battery hardware required to run it.

---

## Project Background

Modern aerospace fire suppression systems rely on battery-powered discharge modules. Monitoring the health of those batteries in real time — voltage, current, temperature, and state of charge — is safety-critical. This project is the firmware prototype for a wireless battery telemetry node that reads battery data over I²C and transmits it over LoRaWAN.

The **mock test harness** in this repository allows the full FES firmware logic to run on a standard STM32 Discovery board without any real battery sensor or LoRa radio attached. All sensor values are generated internally as mathematical waveforms, enabling complete validation of the alarm logic, LED indicators, and serial telemetry output on a simple breadboard.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | STM32L072CZY6TR (Cortex-M0+, 192 KB Flash, 20 KB RAM) |
| Development board | STM32L072 Discovery — MB1296D |
| Target hardware (future) | STM32U585RIT6 with RRC2037 smart battery and RAK11160 LoRaWAN module |
| LED — Green | External LED + 1 kΩ resistor → PA5 (or onboard LD2) |
| LED — Red | External LED + 1 kΩ resistor → PA9 |
| Debug output | USART2 (PA2) → ST-Link virtual COM port → PuTTY at 115200 baud |

---

## Firmware Architecture

```
Core/
├── Inc/
│   ├── FesNode.hpp          # Core driver — RRC2037 I²C + RAK11160 LoRaWAN
│   ├── FesNode_Mock.hpp     # Mock sensor data (no hardware needed)
│   └── fes_led_test.hpp     # LED pin defines and timing constants
└── Src/
    ├── FesNode.cpp          # Driver implementation with #ifdef mock guards
    ├── fes_led_test.cpp     # LED test harness — calls FesNode, drives LEDs
    └── main.c               # CubeMX-generated entry point
```

### Key design principle

`FesNode.cpp` is the real driver. Every hardware-touching method (`rrcInit`, `rrcReadU8`, `rrcReadU16`, `rrcReadI16`, `rakInit`, `rakSendHexPayload`) contains an `#ifdef FES_MOCK_SENSORS` guard. When the mock define is active, those methods return fake data from `FesNode_Mock.hpp` instead of touching I²C or UART. **Zero code changes are needed to switch between mock and real hardware** — only the preprocessor symbol changes.

---

## Mock Sensor Waveforms

All mock values are driven by `HAL_GetTick()` elapsed time, producing repeating waveforms that automatically exercise every alarm threshold:

| Channel | Waveform | Range | Period | Alarm triggered |
|---|---|---|---|---|
| Voltage | Sine wave | 20–35 V | 60 s | OVERVOLTAGE > 32 V |
| Current | Sawtooth ramp | 0–30 A | 40 s | OVERCURRENT > 25 A |
| Temperature | Sine wave | 20–95 °C | 80 s | OVERTEMP > 85 °C |
| State of charge | Linear decay | 100→0 % | 120 s | LOW_SOC < 20 % |

---

## Alarm Flags

Each measurement cycle evaluates eight alarm conditions packed into a single byte:

| Bit | Flag | Condition |
|---|---|---|
| 0x01 | OVERVOLTAGE | V > 32,000 mV (mock) / 8,600 mV (real) |
| 0x02 | UNDERVOLTAGE | V < 20,000 mV (mock) / 6,000 mV (real) |
| 0x04 | OVERCURRENT | I > 25,000 mA (mock) / 4,000 mA (real) |
| 0x08 | OVERTEMP | T > 85.0 °C |
| 0x10 | UNDERTEMP | T < −40.0 °C |
| 0x20 | LOW_SOC | SOC < 20 % |
| 0x40 | SENSOR_FAULT | I²C read failure |
| 0x80 | LORA_FAULT | LoRa TX failure |

---

## LED Behaviour

| Event | Green LED | Red LED |
|---|---|---|
| Power-on self-test | Flash once | Flash once |
| Sensor OK, no alarms | Solid 1 s | Off |
| Any alarm active | Off | Solid 1 s |
| LoRa TX success | 3× blink | Off |
| I²C / sensor fault | Off | 5× fast blink |

---

## LoRa Packet Format

15-byte payload transmitted on LoRaWAN port 2 (unconfirmed uplink):

```
Byte  Field         Type     Unit
[0]   moduleId      uint8    —
[1-2] sequence      uint16   —
[3-4] voltage_mV    uint16   mV
[5-6] current_mA    int16    mA  (negative = charging)
[7-8] temp_dC       int16    °C × 10
[9]   soc_pct       uint8    %
[10]  alarmFlags    uint8    bitmask (see above)
[11-14] timestampMs uint32   ms since boot
```

All multi-byte fields are little-endian (struct layout).

---

## Serial Debug Output

Connect PuTTY (or any terminal) to the ST-Link virtual COM port at **115200 baud, 8N1, no flow control**. Example output:

```
=== FES LED TEST — STM32L072CZY6TR ===
[MODE] MOCK — no real I2C hardware needed
[INIT] OK
[OK]   Sampling every 10 s

[DATA] seq=12    V=33995 mV  I=+22500 mA  T=+30.9°C  SOC= 92%  alarms=0x05
[ALRM] OVERVOLTAGE OVERCURRENT
[LED]  RED — alarm active
[LORA] Sending...
[LORA] OK
[LED]  GREEN 3× — TX OK
```

---

## How to Build

1. Open **STM32CubeIDE 2.x**
2. Import the project: `File → Import → General → Existing Projects into Workspace`
3. Navigate to the project folder → Finish
4. Confirm `FES_MOCK_SENSORS` is in the preprocessor symbols:
   `Project → Properties → C/C++ Build → Settings → MCU GCC Compiler → Preprocessor`
5. `Project → Build Project`
6. Click the green **Run** button to flash

No external hardware is required. The board runs standalone using mock data.

---

## Switching to Real Hardware

To target the real RRC2037 battery and RAK11160 LoRaWAN module:

1. Remove `FES_MOCK_SENSORS` from the preprocessor symbols
2. Connect the RRC2037 to I²C1 (PB6=SCL, PB7=SDA) with 4.7 kΩ pull-ups
3. Pre-provision the RAK11160 with DEVEUI, APPEUI, and APPKEY via AT commands
4. Connect the LoRa antenna before powering the RAK11160
5. Rebuild and flash

---

## Target Hardware — STM32U585RIT6

The production PCB uses the STM32U585RIT6 (Cortex-M33, TrustZone, 2 MB Flash). A separate driver `FesNode_U585.hpp/.cpp` is included for that target with the correct pin mapping:

| Peripheral | Pins | Function |
|---|---|---|
| I2C2 | PB12/PB13 | RRC2037 battery (internal I²C) |
| USART5 | PC12/PD2 | RAK11160 LoRaWAN |
| USART1 | PB6/PB7 | Debug UART |
| GPIO | PC8 | EN_3V3_RAK (active HIGH) |
| GPIO | PC9 | ST_BOOT (LOW = normal) |
| GPIO | PC10 | #WAKE_OUT (active LOW) |

---

## License

This project is proprietary aerospace firmware. All rights reserved.
